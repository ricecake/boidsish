#pragma once

#include <vector>
#include <memory>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "IManager.h"
#include "constants.h"

class ComputeShader;

namespace Boidsish {

	struct VolumetricCascade {
		GLuint texture = 0;
		float  near_dist = 0.0f;
		float  far_dist = 0.0f;
	};

	struct alignas(16) VolumetricLightingUbo {
		glm::mat4 inv_view_proj;
		glm::vec4 grid_size;      // x, y, z, num_cascades
		glm::vec4 clip_params;    // x=near, y=far, z=log_base, w=unused
		glm::vec4 cascade_fars;   // x, y, z, w (matches std140 array layout)
	};

	class VolumetricLightingManager : public IManager {
	public:
		VolumetricLightingManager();
		~VolumetricLightingManager();

		void Initialize() override;
		void Update(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos, const glm::vec3& camera_front, float delta_time);

		GLuint GetCascadeTexture(int index) const { return cascades_[index].texture; }

		int GetNumCascades() const { return static_cast<int>(cascades_.size()); }

	private:
		void CreateTextures();
		void CreateBuffers();
		void CreateShaders();

		std::vector<VolumetricCascade> cascades_;
		GLuint                         froxel_data_ssbo_ = 0;
		GLuint                         lighting_ubo_ = 0;

		std::unique_ptr<ComputeShader> grid_init_shader_;

		glm::mat4 last_projection_{0.0f};
		glm::vec3 last_camera_pos_{0.0f};
		glm::vec3 last_camera_front_{0.0f};
		bool      initialized_ = false;

		uint32_t  frame_counter_ = 0;

		const int grid_x_ = Constants::Class::VolumetricLighting::GridSizeX();
		const int grid_y_ = Constants::Class::VolumetricLighting::GridSizeY();
		const int grid_z_ = Constants::Class::VolumetricLighting::GridSizeZ();
		const int num_cascades_ = Constants::Class::VolumetricLighting::MaxCascades();
	};

} // namespace Boidsish
