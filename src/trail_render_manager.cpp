#include "trail_render_manager.h"

#include <algorithm>

#include "service_locator.h"
#include <cstring>
#include <iostream>
#include <vector>

#include "frustum.h"
#include "logger.h"
#include "profiler.h"
#include "trail.h"
#include <shader.h>

namespace Boidsish {

	TrailRenderManager::TrailRenderManager(ServiceLocator& /*loc*/) {
		// Initialize tessellation shader
		tess_shader_ = std::make_unique<ComputeShader>("shaders/trail_tess.comp");

		// Create VAO for rendering generated geometry
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);

		// Create VBO for tessellated vertices
		// Max trails (e.g. 200) * Max rings (1024) * Verts per ring (9)
		// Each vertex has 3 vec4s: Position(4) + Normal(4) + Color(4) = 12 floats
		size_t vbo_elements = 200 * 1024 * 9 * 12;
		tess_vbo_ = std::make_unique<PersistentBuffer<float>>(GL_ARRAY_BUFFER, vbo_elements, 3);
		glBindBuffer(GL_ARRAY_BUFFER, tess_vbo_->GetBufferId());

		// Set up vertex attributes (matches TrailVertex in trail_common.glsl)
		// pos (location 0)
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
		// normal (location 1)
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(4 * sizeof(float)));
		// color (location 2)
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float)));

		// Create EBO with static index pattern for tube segments (triangle strip-like via triangles)
		const int TRAIL_SEGMENTS = 8;
		const int MAX_RINGS = 1024;
		size_t    ebo_elements = (MAX_RINGS - 1) * TRAIL_SEGMENTS * 6;
		tess_ebo_ = std::make_unique<PersistentBuffer<uint32_t>>(GL_ELEMENT_ARRAY_BUFFER, ebo_elements, 1);
		uint32_t* indices_ptr = tess_ebo_->GetFullBufferPtr();

		int offset = 0;
		for (int r = 0; r < MAX_RINGS - 1; ++r) {
			for (int s = 0; s < TRAIL_SEGMENTS; ++s) {
				uint32_t curr_ring = r * (TRAIL_SEGMENTS + 1);
				uint32_t next_ring = (r + 1) * (TRAIL_SEGMENTS + 1);

				// CCW Winding: (curr, s) -> (next, s+1) -> (next, s)
				indices_ptr[offset++] = curr_ring + s;
				indices_ptr[offset++] = next_ring + s + 1;
				indices_ptr[offset++] = next_ring + s;

				// CCW Winding: (curr, s) -> (curr, s+1) -> (next, s+1)
				indices_ptr[offset++] = curr_ring + s;
				indices_ptr[offset++] = curr_ring + s + 1;
				indices_ptr[offset++] = next_ring + s + 1;
			}
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tess_ebo_->GetBufferId());

		glBindVertexArray(0);

		// Create SSBOs for points and metadata
		points_ssbo_ = std::make_unique<PersistentBuffer<TrailPoint>>(GL_SHADER_STORAGE_BUFFER, INITIAL_POINTS_CAPACITY, 3);
		points_capacity_ = INITIAL_POINTS_CAPACITY;

		instances_ssbo_ = std::make_unique<PersistentBuffer<TrailInstance>>(GL_SHADER_STORAGE_BUFFER, 200, 3);

		// Create Spine SSBO
		// Max trails (200) * Max rings (1024)
		spine_ssbo_ = std::make_unique<PersistentBuffer<TrailSpinePoint>>(GL_SHADER_STORAGE_BUFFER, 200 * 1024, 3);
	}

	TrailRenderManager::~TrailRenderManager() {
		if (vao_)
			glDeleteVertexArrays(1, &vao_);
	}

	bool TrailRenderManager::RegisterTrail(int trail_id, size_t max_points) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (trail_allocations_.count(trail_id))
			return false;

		size_t points_offset = points_usage_;
		for (auto it = free_list_.begin(); it != free_list_.end(); ++it) {
			if (it->size >= max_points) {
				points_offset = it->offset;
				if (it->size == max_points) {
					free_list_.erase(it);
				} else {
					it->offset += max_points;
					it->size -= max_points;
				}
				break;
			}
		}

		if (points_offset == points_usage_) {
			points_usage_ += max_points;
		}

		if (points_usage_ > points_capacity_) {
			EnsureBufferCapacity(points_usage_);
		}

		TrailAllocation allocation{};
		allocation.points_offset = points_offset;
		allocation.max_points = max_points;

		if (!free_vertex_slots_.empty()) {
			allocation.vertex_offset = free_vertex_slots_.back();
			free_vertex_slots_.pop_back();
		} else {
			allocation.vertex_offset = next_vertex_slot_ * 1024 * 9;
			next_vertex_slot_++;
		}

		trail_allocations_[trail_id] = allocation;
		return true;
	}

	void TrailRenderManager::UnregisterTrail(int trail_id) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto                        it = trail_allocations_.find(trail_id);
		if (it == trail_allocations_.end())
			return;

		free_list_.push_back({it->second.points_offset, it->second.max_points});
		free_vertex_slots_.push_back(it->second.vertex_offset);
		trail_allocations_.erase(it);
		pending_point_data_.erase(trail_id);
	}

	bool TrailRenderManager::HasTrail(int trail_id) const {
		std::lock_guard<std::mutex> lock(mutex_);
		return trail_allocations_.count(trail_id) > 0;
	}

	void TrailRenderManager::UpdateTrailData(
		int                                                trail_id,
		const std::deque<std::pair<glm::vec3, glm::vec3>>& points,
		size_t                                             head,
		size_t                                             tail,
		bool                                               is_full,
		const glm::vec3&                                   min_bound,
		const glm::vec3&                                   max_bound
	) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto                        it = trail_allocations_.find(trail_id);
		if (it == trail_allocations_.end())
			return;

		auto&              alloc = it->second;
		std::vector<float> raw_points;
		raw_points.reserve(points.size() * 8);
		for (const auto& p : points) {
			raw_points.push_back(p.first.x);
			raw_points.push_back(p.first.y);
			raw_points.push_back(p.first.z);
			raw_points.push_back(1.0f);
			raw_points.push_back(p.second.x);
			raw_points.push_back(p.second.y);
			raw_points.push_back(p.second.z);
			raw_points.push_back(1.0f);
		}

		pending_point_data_[trail_id] = std::move(raw_points);
		alloc.head = head;
		alloc.tail = tail;
		alloc.is_full = is_full;
		alloc.min_bound = min_bound;
		alloc.max_bound = max_bound;
		alloc.needs_upload = true;
	}

	void TrailRenderManager::SetTrailParams(
		int   trail_id,
		bool  iridescent,
		bool  rocket_trail,
		bool  use_pbr,
		float roughness,
		float metallic,
		float base_thickness
	) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto                        it = trail_allocations_.find(trail_id);
		if (it == trail_allocations_.end())
			return;

		auto& alloc = it->second;
		alloc.iridescent = iridescent;
		alloc.rocket_trail = rocket_trail;
		alloc.use_pbr = use_pbr;
		alloc.roughness = roughness;
		alloc.metallic = metallic;
		alloc.base_thickness = base_thickness;
	}

	void TrailRenderManager::EnsureBufferCapacity(size_t required_points) {
		if (required_points <= points_capacity_)
			return;
		size_t new_capacity = static_cast<size_t>(points_capacity_ * GROWTH_FACTOR);
		while (new_capacity < required_points)
			new_capacity = static_cast<size_t>(new_capacity * GROWTH_FACTOR);

		auto new_ssbo = std::make_unique<PersistentBuffer<TrailPoint>>(GL_SHADER_STORAGE_BUFFER, new_capacity, 3);

		memcpy(new_ssbo->GetFullBufferPtr(), points_ssbo_->GetFullBufferPtr(), points_ssbo_->GetTotalSize());

		points_ssbo_ = std::move(new_ssbo);
		points_capacity_ = new_capacity;
	}

	void TrailRenderManager::CommitUpdates() {
		PROJECT_PROFILE_SCOPE("TrailRenderManager::CommitUpdates");
		std::lock_guard<std::mutex> lock(mutex_);
		if (trail_allocations_.empty())
			return;

		points_ssbo_->AdvanceFrame();
		instances_ssbo_->AdvanceFrame();
		spine_ssbo_->AdvanceFrame();
		tess_vbo_->AdvanceFrame();

		// 1. Upload points and metadata
		for (auto& [id, data] : pending_point_data_) {
			auto it = trail_allocations_.find(id);
			if (it != trail_allocations_.end()) {
				TrailPoint* dst = points_ssbo_->GetFrameDataPtr() + it->second.points_offset;
				memcpy(dst, data.data(), data.size() * sizeof(float));
			}
		}
		pending_point_data_.clear();

		TrailInstance* instance_ptr = instances_ssbo_->GetFrameDataPtr();
		int            idx = 0;
		for (const auto& [id, alloc] : trail_allocations_) {
			TrailInstance& inst = instance_ptr[idx++];
			inst.points_offset = static_cast<uint32_t>(alloc.points_offset);
			inst.head = static_cast<uint32_t>(alloc.head);
			inst.tail = static_cast<uint32_t>(alloc.tail);
			inst.max_points = static_cast<uint32_t>(alloc.max_points);
			inst.thickness = alloc.base_thickness;
			inst.vertex_offset = static_cast<uint32_t>(alloc.vertex_offset);
			inst.is_full = alloc.is_full ? 1 : 0;

			inst.flags = 0;
			if (alloc.iridescent)
				inst.flags |= 1;
			if (alloc.rocket_trail)
				inst.flags |= 2;
			if (alloc.use_pbr)
				inst.flags |= 4;

			inst.roughness = alloc.roughness;
			inst.metallic = alloc.metallic;
		}

		// 2. Dispatch tessellation compute shader
		tess_shader_->use();
		tess_shader_->setInt("u_num_instances", (int)trail_allocations_.size());
		points_ssbo_->BindRange(Constants::SsboBinding::TrailPoints());
		instances_ssbo_->BindRange(Constants::SsboBinding::TrailInstances());
		spine_ssbo_->BindRange(Constants::SsboBinding::TrailSpineData());
		tess_vbo_->BindRange(Constants::SsboBinding::TrailGeneratedVBO());

		glDispatchCompute(trail_allocations_.size(), 1, 1);
		glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
	}

	void TrailRenderManager::GetRenderPackets(
		std::vector<RenderPacket>& out_packets,
		const RenderContext&       context,
		ShaderHandle               shader_handle
	) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (trail_allocations_.empty())
			return;

		for (const auto& [id, alloc] : trail_allocations_) {
			if (!context.frustum.IsBoxInFrustum(alloc.min_bound, alloc.max_bound)) {
				continue;
			}

			size_t num_points = alloc.is_full ? alloc.max_points : alloc.tail;
			if (num_points < 2) {
				continue;
			}

			uint32_t num_rings = (num_points - 1) * 4 + 1; // CURVE_SEGMENTS = 4
			if (num_rings > 1024)
				num_rings = 1024;

			RenderPacket packet;
			packet.shader_handle = shader_handle;
			packet.material_handle = MaterialHandle(0); // Standard trail material
			packet.vao = vao_;
			packet.vbo = tess_vbo_->GetBufferId();
			packet.vbo_offset = static_cast<uint32_t>(tess_vbo_->GetFrameOffset() / (12 * sizeof(float)));
			packet.ebo = tess_ebo_->GetBufferId();
			packet.shader_id = shader_handle.id;
			packet.index_count = (num_rings - 1) * 8 * 6;
			packet.first_index = 0;
			packet.base_vertex = alloc.vertex_offset;
			packet.draw_mode = GL_TRIANGLES;
			packet.index_type = GL_UNSIGNED_INT;
			packet.casts_shadows = true;

			// Set uniforms
			packet.uniforms.model = glm::mat4(1.0f);
			packet.uniforms.color = glm::vec4(1.0f); // Colors are in vertex data
			packet.uniforms.use_pbr = alloc.use_pbr ? 1 : 0;
			packet.uniforms.roughness = alloc.roughness;
			packet.uniforms.metallic = alloc.metallic;
			packet.uniforms.ao = alloc.base_thickness; // Pass thickness via AO field
			packet.uniforms.use_vertex_color = 1;

			// Add custom flags for trail effects
			// We can repurpose some unused fields in CommonUniforms or use the 'flags' concept
			// For now, let's use is_line and line_style for some effects
			packet.uniforms.is_line = (alloc.iridescent ? 1 : 0) | (alloc.rocket_trail ? 2 : 0);

			float normalized_depth = context.CalculateNormalizedDepth((alloc.min_bound + alloc.max_bound) * 0.5f);
			packet.sort_key = CalculateSortKey(
				RenderLayer::Transparent,
				shader_handle,
				vao_,
				GL_TRIANGLES,
				true,
				packet.material_handle,
				normalized_depth
			);

			out_packets.push_back(std::move(packet));
		}
	}

	size_t TrailRenderManager::GetRegisteredTrailCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return trail_allocations_.size();
	}

	size_t TrailRenderManager::GetTotalVertexCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		size_t                      total = 0;
		for (const auto& [id, alloc] : trail_allocations_) {
			size_t num_points = alloc.is_full ? alloc.max_points : alloc.tail;
			if (num_points >= 2) {
				uint32_t num_rings = (num_points - 1) * 4 + 1;
				total += num_rings * 9;
			}
		}
		return total;
	}

} // namespace Boidsish
