#pragma once

#include <memory>
#include <string>
#include <vector>

#include "model.h"
#include <glm/glm.hpp>
#include <shader.h>

namespace Boidsish {

	class TerrainGenerator;
	struct Camera;

	struct DecorType {
		std::shared_ptr<Model> model;
		float                  density = 1.0f;
		float                  min_height = -100.0f;
		float                  max_height = 1000.0f;
		float                  min_slope = 0.0f;
		float                  max_slope = 1.0f;
		float                  base_scale = 1.0f;
		float                  scale_variance = 0.2f;

		// GPU resources
		unsigned int ssbo = 0;
		unsigned int count_buffer = 0; // For atomic counter
	};

	class DecorManager {
	public:
		DecorManager();
		~DecorManager();

		void AddDecorType(const std::string& model_path, float density);
		void Update(float delta_time, const Camera& camera, const TerrainGenerator& terrain_gen);
		void Render(const glm::mat4& view, const glm::mat4& projection);

		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }

	private:
		void _Initialize();

		bool                               enabled_ = true;
		bool                               initialized_ = false;
		std::vector<DecorType>             decor_types_;
		std::unique_ptr<ComputeShader>     placement_shader_;
		std::shared_ptr<Shader>            render_shader_; // Maybe use vis.vert/frag or a specialized one

		unsigned int heightmap_texture_ = 0;
		glm::vec2    heightmap_world_pos_ = {0.0f, 0.0f};
		float        heightmap_size_ = 1024.0f;

		static constexpr int kMaxInstancesPerType = 10000;
	};

} // namespace Boidsish
