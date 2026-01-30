#pragma once

#include <memory>
#include <string>
#include <vector>

#include "frustum.h"
#include "model.h"
#include <glm/glm.hpp>
#include <shader.h>

namespace Boidsish {

	class TerrainGenerator;
	class TerrainRenderManager;
	struct Camera;

	// Properties for configuring decor placement and appearance
	struct DecorProperties {
		float     density = 1.0f;                  // Probability of placement (0-1)
		float     base_scale = 1.0f;               // Base scale factor
		float     scale_variance = 0.2f;           // Random scale variation (+/-)
		float     min_height = -100.0f;            // Minimum terrain height for placement
		float     max_height = 1000.0f;            // Maximum terrain height for placement
		float     min_slope = 0.0f;                // Minimum terrain slope (0=flat)
		float     max_slope = 1.0f;                // Maximum terrain slope (1=vertical)
		glm::vec3 base_rotation = glm::vec3(0.0f); // Base rotation in degrees (pitch, yaw, roll)
		bool      random_yaw = true;               // Apply random Y rotation
		bool      align_to_terrain =
			false; // If true, align to terrain normal (bush on cliff); if false, align to world up (tree)
	};

	struct DecorType {
		std::shared_ptr<Model> model;
		DecorProperties        props;

		// GPU resources
		unsigned int ssbo = 0;
		unsigned int count_buffer = 0; // For atomic counter

		// Cached instance count (read back after compute, used during render)
		unsigned int cached_count = 0;
	};

	class DecorManager {
	public:
		DecorManager();
		~DecorManager();

		// Simple overload for basic usage
		void AddDecorType(const std::string& model_path, float density);

		// Full overload with all properties
		void AddDecorType(const std::string& model_path, const DecorProperties& props);

		void Update(
			float                                 delta_time,
			const Camera&                         camera,
			const Frustum&                        frustum,
			const TerrainGenerator&               terrain_gen,
			std::shared_ptr<TerrainRenderManager> render_manager
		);
		void Render(const glm::mat4& view, const glm::mat4& projection);

		void SetEnabled(bool enabled) { enabled_ = enabled; }

		bool IsEnabled() const { return enabled_; }

	private:
		void _Initialize();
		void _RegeneratePlacements(
			const Camera&                         camera,
			const Frustum&                        frustum,
			const TerrainGenerator&               terrain_gen,
			std::shared_ptr<TerrainRenderManager> render_manager
		);

		bool                           enabled_ = true;
		bool                           initialized_ = false;
		std::vector<DecorType>         decor_types_;
		std::unique_ptr<ComputeShader> placement_shader_;
		std::shared_ptr<Shader>        render_shader_; // Maybe use vis.vert/frag or a specialized one

		// Caching - only regenerate when camera moves significantly or rotates
		glm::vec2              last_camera_pos_ = glm::vec2(0.0f);
		glm::vec3              last_camera_front_ = glm::vec3(0.0f, 0.0f, -1.0f);
		bool                   needs_regeneration_ = true;
		static constexpr float kRegenerationDistance = 20.0f; // Regenerate when camera moves this far
		static constexpr float kRegenerationAngle = 0.15f;    // ~8.5 degrees (1 - cos(angle))

		static constexpr int kMaxInstancesPerType = 10000;
	};

} // namespace Boidsish
