#include "trail_render_manager.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

#include "frustum.h"
#include "logger.h"
#include "profiler.h"
#include "trail.h"
#include <shader.h>

namespace Boidsish {

	TrailRenderManager::TrailRenderManager() {
		// Initialize tessellation shader
		tess_shader_ = std::make_unique<ComputeShader>("shaders/trail_tess.comp");

		// Create VAO for rendering generated geometry
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);

		// Create VBO for tessellated vertices
		glGenBuffers(1, &tess_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, tess_vbo_);
		// Max trails (e.g. 200) * Max rings (1024) * Verts per ring (9)
		size_t vbo_size = 200 * 1024 * 9 * 4 * sizeof(float) * 3; // Position(4) + Normal(4) + Color(4)
		glBufferData(GL_ARRAY_BUFFER, vbo_size, nullptr, GL_DYNAMIC_DRAW);

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
		glGenBuffers(1, &tess_ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tess_ebo_);

		const int             TRAIL_SEGMENTS = 8;
		const int             MAX_RINGS = 1024;
		std::vector<uint32_t> indices;
		indices.reserve((MAX_RINGS - 1) * TRAIL_SEGMENTS * 6);

		for (int r = 0; r < MAX_RINGS - 1; ++r) {
			for (int s = 0; s < TRAIL_SEGMENTS; ++s) {
				uint32_t curr_ring = r * (TRAIL_SEGMENTS + 1);
				uint32_t next_ring = (r + 1) * (TRAIL_SEGMENTS + 1);

				// CCW Winding: (curr, s) -> (next, s+1) -> (next, s)
				indices.push_back(curr_ring + s);
				indices.push_back(next_ring + s + 1);
				indices.push_back(next_ring + s);

				// CCW Winding: (curr, s) -> (curr, s+1) -> (next, s+1)
				indices.push_back(curr_ring + s);
				indices.push_back(curr_ring + s + 1);
				indices.push_back(next_ring + s + 1);
			}
		}
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

		glBindVertexArray(0);

		// Create SSBOs for points and metadata
		glGenBuffers(1, &points_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, points_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, INITIAL_POINTS_CAPACITY * 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
		points_capacity_ = INITIAL_POINTS_CAPACITY;

		glGenBuffers(1, &instances_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, instances_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 200 * 12 * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		// Create Spine SSBO
		glGenBuffers(1, &spine_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, spine_ssbo_);
		// Max trails (200) * Max rings (1024) * TrailSpinePoint size (4 vec4 = 64 bytes)
		glBufferData(GL_SHADER_STORAGE_BUFFER, 200 * 1024 * 64, nullptr, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	TrailRenderManager::~TrailRenderManager() {
		if (vao_)
			glDeleteVertexArrays(1, &vao_);
		if (tess_vbo_)
			glDeleteBuffers(1, &tess_vbo_);
		if (tess_ebo_)
			glDeleteBuffers(1, &tess_ebo_);
		if (points_ssbo_)
			glDeleteBuffers(1, &points_ssbo_);
		if (instances_ssbo_)
			glDeleteBuffers(1, &instances_ssbo_);
		if (spine_ssbo_)
			glDeleteBuffers(1, &spine_ssbo_);
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

		GLuint new_ssbo;
		glGenBuffers(1, &new_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, new_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, new_capacity * 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_COPY_READ_BUFFER, points_ssbo_);
		glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_SHADER_STORAGE_BUFFER, 0, 0, points_capacity_ * 8 * sizeof(float));

		glDeleteBuffers(1, &points_ssbo_);
		points_ssbo_ = new_ssbo;
		points_capacity_ = new_capacity;
	}

	void TrailRenderManager::CommitUpdates() {
		PROJECT_PROFILE_SCOPE("TrailRenderManager::CommitUpdates");
		std::lock_guard<std::mutex> lock(mutex_);
		if (trail_allocations_.empty())
			return;

		// 1. Upload points and metadata
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, points_ssbo_);
		for (auto& [id, data] : pending_point_data_) {
			auto it = trail_allocations_.find(id);
			if (it != trail_allocations_.end()) {
				glBufferSubData(
					GL_SHADER_STORAGE_BUFFER,
					it->second.points_offset * 8 * sizeof(float),
					data.size() * sizeof(float),
					data.data()
				);
			}
		}
		pending_point_data_.clear();

		std::vector<uint32_t> instance_data;
		instance_data.reserve(trail_allocations_.size() * 12);
		for (const auto& [id, alloc] : trail_allocations_) {
			instance_data.push_back(static_cast<uint32_t>(alloc.points_offset));
			instance_data.push_back(static_cast<uint32_t>(alloc.head));
			instance_data.push_back(static_cast<uint32_t>(alloc.tail));
			instance_data.push_back(static_cast<uint32_t>(alloc.max_points));

			float    thickness = alloc.base_thickness;
			uint32_t thickness_bits;
			std::memcpy(&thickness_bits, &thickness, 4);
			instance_data.push_back(thickness_bits);

			instance_data.push_back(static_cast<uint32_t>(alloc.vertex_offset));
			instance_data.push_back(alloc.is_full ? 1 : 0);

			uint32_t flags = 0;
			if (alloc.iridescent)
				flags |= 1;
			if (alloc.rocket_trail)
				flags |= 2;
			if (alloc.use_pbr)
				flags |= 4;
			instance_data.push_back(flags);

			float    roughness = alloc.roughness;
			uint32_t roughness_bits;
			std::memcpy(&roughness_bits, &roughness, 4);
			instance_data.push_back(roughness_bits);

			float    metallic = alloc.metallic;
			uint32_t metallic_bits;
			std::memcpy(&metallic_bits, &metallic, 4);
			instance_data.push_back(metallic_bits);

			instance_data.push_back(0); // padding
			instance_data.push_back(0); // padding
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, instances_ssbo_);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, instance_data.size() * sizeof(uint32_t), instance_data.data());

		// 2. Dispatch tessellation compute shader
		tess_shader_->use();
		tess_shader_->setInt("u_num_instances", trail_allocations_.size());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 18, points_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 19, instances_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, spine_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tess_vbo_);

		glDispatchCompute(trail_allocations_.size(), 1, 1);
		glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
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
			packet.ebo = tess_ebo_;
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
