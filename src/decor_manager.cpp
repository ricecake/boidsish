#include "decor_manager.h"

#include <algorithm>

#include "ConfigManager.h"
#include "graphics.h"
#include "logger.h"
#include "terrain_generator.h"
#include "terrain_render_manager.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	DecorManager::DecorManager() {}

	DecorManager::~DecorManager() {
		for (auto& type : decor_types_) {
			if (type.ssbo != 0)
				glDeleteBuffers(1, &type.ssbo);
			if (type.count_buffer != 0)
				glDeleteBuffers(1, &type.count_buffer);
		}
	}

	void DecorManager::_Initialize() {
		if (initialized_)
			return;

		placement_shader_ = std::make_unique<ComputeShader>("shaders/decor_placement.comp");

		// Check if compute shader compiled successfully
		if (!placement_shader_->isValid()) {
			logger::ERROR("Failed to compile decor placement compute shader - decor will be disabled");
			initialized_ = true; // Mark as initialized to prevent repeated attempts
			return;
		}

		initialized_ = true;
	}

	void DecorManager::AddDecorType(const std::string& model_path, float density) {
		DecorProperties props;
		props.SetDensity(density);
		AddDecorType(model_path, props);
	}

	void DecorManager::AddDecorType(const std::string& model_path, const DecorProperties& props) {
		_Initialize();

		DecorType type;
		type.model = std::make_shared<Model>(model_path);
		type.props = props;

		glGenBuffers(1, &type.ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.ssbo);
		// Each instance is a mat4 (64 bytes)
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxInstancesPerType * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &type.count_buffer);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

		decor_types_.push_back(type);
	}

	void DecorManager::Update(
		float /*delta_time*/,
		const Camera&                         camera,
		const Frustum&                        frustum,
		const TerrainGenerator&               terrain_gen,
		std::shared_ptr<TerrainRenderManager> render_manager
	) {
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		if (!placement_shader_ || !placement_shader_->isValid())
			return;

		if (!render_manager)
			return;

		// Check if we need to regenerate placements
		glm::vec2 cam_pos(camera.x, camera.z);
		glm::vec3 cam_front = camera.front();
		float     dist_moved = glm::distance(cam_pos, last_camera_pos_);

		// Check rotation: 1 - dot(a, b) gives 0 when same direction, ~2 when opposite
		float angle_change = 1.0f - glm::dot(cam_front, last_camera_front_);

		if (needs_regeneration_ || dist_moved > kRegenerationDistance || angle_change > kRegenerationAngle) {
			_RegeneratePlacements(camera, frustum, terrain_gen, render_manager);
			last_camera_pos_ = cam_pos;
			last_camera_front_ = cam_front;
			needs_regeneration_ = false;
		}
	}

	void DecorManager::_RegeneratePlacements(
		const Camera&                         camera,
		const Frustum&                        frustum,
		const TerrainGenerator&               terrain_gen,
		std::shared_ptr<TerrainRenderManager> render_manager
	) {
		// Get the heightmap texture array from the terrain render manager
		GLuint heightmap_texture = render_manager->GetHeightmapTexture();
		if (heightmap_texture == 0)
			return;

		// Get chunk info (world_offset_x, world_offset_z, texture_slice, chunk_size)
		auto chunk_info = render_manager->GetChunkInfo();
		if (chunk_info.empty())
			return;

		glm::vec2 cam_pos(camera.x, camera.z);
		glm::vec3 cam_pos_3d(camera.x, camera.y, camera.z);
		glm::vec3 cam_front = camera.front();
		glm::vec2 cam_front_xz = glm::normalize(glm::vec2(cam_front.x, cam_front.z));

		placement_shader_->use();
		placement_shader_->setVec2("u_cameraPos", cam_pos);
		placement_shader_->setFloat("u_maxTerrainHeight", terrain_gen.GetMaxHeight());

		// Distance-based density falloff parameters
		placement_shader_->setFloat("u_densityFalloffStart", density_falloff_start_);
		placement_shader_->setFloat("u_densityFalloffEnd", density_falloff_end_);
		placement_shader_->setFloat("u_maxDecorDistance", max_decor_distance_);

		// Pass frustum planes for GPU-side culling
		for (int p = 0; p < 6; ++p) {
			placement_shader_->setVec4(
				"u_frustumPlanes[" + std::to_string(p) + "]",
				glm::vec4(frustum.planes[p].normal, frustum.planes[p].distance)
			);
		}

		// Bind heightmap texture array
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture);
		placement_shader_->setInt("u_heightmapArray", 0);

		// Filter chunks: only process chunks that intersect the frustum
		// and sort by priority (closer/in-front first)
		std::vector<std::pair<float, size_t>> chunk_priorities;
		chunk_priorities.reserve(chunk_info.size());

		for (size_t ci = 0; ci < chunk_info.size(); ++ci) {
			const auto& chunk = chunk_info[ci];
			glm::vec2   chunk_offset(chunk.x, chunk.y);
			float       chunk_size = chunk.w;
			glm::vec2   chunk_center = chunk_offset + glm::vec2(chunk_size * 0.5f);

			float dist = glm::distance(cam_pos, chunk_center);
			if (dist > max_decor_distance_)
				continue;

			// Frustum cull the chunk (approximate as AABB)
			// Use a generous height range since we don't know exact decor heights
			glm::vec3 chunk_min(chunk_offset.x, -50.0f, chunk_offset.y);
			glm::vec3 chunk_max(
				chunk_offset.x + chunk_size,
				terrain_gen.GetMaxHeight() + 50.0f,
				chunk_offset.y + chunk_size
			);

			bool inside = true;
			for (int p = 0; p < 6 && inside; ++p) {
				const auto& plane = frustum.planes[p];
				// Find the corner most in the direction of the plane normal
				glm::vec3 positive_vertex(
					plane.normal.x >= 0 ? chunk_max.x : chunk_min.x,
					plane.normal.y >= 0 ? chunk_max.y : chunk_min.y,
					plane.normal.z >= 0 ? chunk_max.z : chunk_min.z
				);
				if (glm::dot(plane.normal, positive_vertex) + plane.distance < 0) {
					inside = false;
				}
			}
			if (!inside)
				continue;

			// Direction to chunk center
			glm::vec2 to_chunk = chunk_center - cam_pos;
			float     to_chunk_len = glm::length(to_chunk);
			if (to_chunk_len > 0.001f) {
				to_chunk /= to_chunk_len;
			}
			float dot = glm::dot(cam_front_xz, to_chunk);

			// Priority: lower is better
			// - Closer chunks get lower priority value
			// - Chunks in front of camera (dot > 0) get lower priority
			float priority = dist * (1.0f - dot * 0.5f);

			chunk_priorities.emplace_back(priority, ci);
		}

		// Sort by priority (lower first)
		std::sort(chunk_priorities.begin(), chunk_priorities.end());

		// Reset all atomic counters first (batch the buffer updates)
		unsigned int zero = 0;
		for (auto& type : decor_types_) {
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
			glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(unsigned int), &zero);
		}

		for (size_t i = 0; i < decor_types_.size(); ++i) {
			auto& type = decor_types_[i];

			placement_shader_->setFloat("u_minDensity", type.props.min_density);
			placement_shader_->setFloat("u_maxDensity", type.props.max_density);
			placement_shader_->setFloat("u_baseScale", type.props.base_scale);
			placement_shader_->setFloat("u_scaleVariance", type.props.scale_variance);
			placement_shader_->setFloat("u_minHeight", type.props.min_height);
			placement_shader_->setFloat("u_maxHeight", type.props.max_height);
			placement_shader_->setFloat("u_minSlope", type.props.min_slope);
			placement_shader_->setFloat("u_maxSlope", type.props.max_slope);
			placement_shader_->setVec3("u_baseRotation", glm::radians(type.props.base_rotation));
			placement_shader_->setBool("u_randomYaw", type.props.random_yaw);
			placement_shader_->setBool("u_alignToTerrain", type.props.align_to_terrain);
			placement_shader_->setInt("u_typeIndex", (int)i);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, type.ssbo);
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, type.count_buffer);

			// Process chunks in priority order (closest/in-front first)
			for (const auto& [priority, ci] : chunk_priorities) {
				const auto& chunk = chunk_info[ci];
				glm::vec2   chunk_offset(chunk.x, chunk.y);
				float       chunk_slice = chunk.z;
				float       chunk_size = chunk.w;

				placement_shader_->setVec2("u_chunkWorldOffset", chunk_offset);
				placement_shader_->setFloat("u_chunkSlice", chunk_slice);
				;
				placement_shader_->setFloat("u_chunkSize", chunk_size);

				// Dispatch compute shader for this chunk
				// 4x4 work groups of 8x8 threads = 32x32 potential placement points per chunk
				glDispatchCompute(4, 4, 1);
			}

			// Memory barrier after each type to ensure all writes complete before next type
			// This prevents race conditions between different decor types
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
		}

		// Read back counts after all compute work is done
		for (size_t i = 0; i < decor_types_.size(); ++i) {
			auto& type = decor_types_[i];
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
			glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(unsigned int), &type.cached_count);
			if (type.cached_count > kMaxInstancesPerType)
				type.cached_count = kMaxInstancesPerType;
		}
	}

	void DecorManager::Render(const glm::mat4& view, const glm::mat4& projection) {
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		auto shader = Shape::shader;
		if (!shader)
			return;

		// Ensure SSBO data from compute shader is visible to vertex shader
		// This barrier is needed because compute wrote to SSBO, and vertex shader reads from it
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		shader->use();
		shader->setMat4("view", view);
		shader->setMat4("projection", projection);
		shader->setMat4("model", glm::mat4(1.0f)); // Identity - instances provide transform
		shader->setBool("useSSBOInstancing", true);
		shader->setBool("isColossal", false);
		shader->setBool("is_instanced", false);
		shader->setVec3("objectColor", 1.0f, 1.0f, 1.0f);
		shader->setBool("usePBR", false);
		shader->setVec4("clipPlane", 0.0f, 0.0f, 0.0f, 0.0f); // Disable clipping for main pass
		shader->setFloat(
			"ripple_strength",
			ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_ripple", false) ? 0.05f : 0.0f
		);

		for (size_t i = 0; i < decor_types_.size(); ++i) {
			auto& type = decor_types_[i];
			// Use cached count from compute pass
			unsigned int count = type.cached_count;

			if (count == 0)
				continue;

			// Bind SSBO to a known binding point that the shader expects for instance matrices
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, type.ssbo);

			size_t mesh_count = type.model->getMeshes().size();

			for (const auto& mesh : type.model->getMeshes()) {
				mesh.bindTextures(*shader);
				mesh.render_instanced((int)count);
			}
		}

		shader->setBool("useSSBOInstancing", false);
	}

} // namespace Boidsish
