#pragma once

#include <memory>
#include <string>
#include <vector>

#include "frustum.h"
#include "model.h"
#include <glm/glm.hpp>
#include <shader.h>

namespace Boidsish {

	class ITerrainGenerator;
	class TerrainRenderManager;
	struct Camera;

	// Properties for configuring decor placement and appearance
	struct DecorProperties {
		float     min_density = 0.1f;              // Minimum density (ensures all visible areas get some decor)
		float     max_density = 1.0f;              // Maximum density at close range
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

		void SetDensity(float d) {
			min_density = d * 0.2f;
			max_density = d;
		}
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

		// Simple overload for basic usage (sets max_density, min_density = max * 0.2)
		void AddDecorType(const std::string& model_path, float density);

		// Full overload with all properties
		void AddDecorType(const std::string& model_path, const DecorProperties& props);

		void Update(
			float                                 delta_time,
			const Camera&                         camera,
			const Frustum&                        frustum,
			const ITerrainGenerator&              terrain_gen,
			std::shared_ptr<TerrainRenderManager> render_manager
		);
		void Render(const glm::mat4& view, const glm::mat4& projection);

		// Render with a specific shader (e.g. for shadow pass)
		void Render(Shader& shader);

		void SetEnabled(bool enabled) { enabled_ = enabled; }

		bool IsEnabled() const { return enabled_; }

		// Distance at which density starts to fall off from max toward min
		void SetDensityFalloffStart(float distance) { density_falloff_start_ = distance; }

		// Distance at which density reaches minimum
		void SetDensityFalloffEnd(float distance) { density_falloff_end_ = distance; }

		// Maximum distance at which decor is placed at all
		void SetMaxDistance(float distance) { max_decor_distance_ = distance; }

		const std::vector<DecorType>& GetDecorTypes() const { return decor_types_; }

	private:
		void _Initialize();
		void _RegeneratePlacements(
			const Camera&                         camera,
			const Frustum&                        frustum,
			const ITerrainGenerator&              terrain_gen,
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
		uint32_t               last_terrain_version_ = 0xFFFFFFFF; // Force initial regen
		float                  last_world_scale_ = 0.0f;
		bool                   needs_regeneration_ = true;
		static constexpr float kRegenerationDistance = 20.0f; // Regenerate when camera moves this far
		static constexpr float kRegenerationAngle = 0.05f;    // ~8.5 degrees (1 - cos(angle))

		// Distance-based density parameters
		float density_falloff_start_ = 10.0f; // Full density within this range
		float density_falloff_end_ = 300.0f;  // Minimum density beyond this range
		float max_decor_distance_ = 600.0f;   // No decor beyond this distance

		static constexpr int kMaxInstancesPerType = 131072;
	};

} // namespace Boidsish
