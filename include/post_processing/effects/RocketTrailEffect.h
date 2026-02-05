#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include "rocket_voxel_tree.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		/**
		 * @brief Post-processing effect for rendering rocket smoke trails
		 *
		 * This effect uses a 3D texture to store voxelized trail data and
		 * raymarches it in the fragment shader to create a "cloudy" look.
		 */
		class RocketTrailEffect: public IPostProcessingEffect {
		public:
			RocketTrailEffect();
			~RocketTrailEffect();

			void Initialize(int width, int height) override;
			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;
			void SetTime(float time) override { time_ = time; }

			/**
			 * @brief Update the GPU voxel data from the CPU tree
			 * @param tree The RocketVoxelTree containing trail data
			 * @param cameraPos Current camera position for grid centering
			 */
			void UpdateVoxels(const RocketVoxelTree& tree, const glm::vec3& cameraPos);

		private:
			void CreateResources();
			void ReleaseResources();

			std::unique_ptr<Shader>        fragment_shader_;
			std::unique_ptr<ComputeShader> clear_shader_;
			std::unique_ptr<ComputeShader> write_shader_;

			GLuint voxel_texture_ = 0;
			GLuint voxel_ssbo_ = 0;

			float time_ = 0.0f;
			int   width_ = 0, height_ = 0;
			int   active_voxel_count_ = 0;

			glm::vec3 grid_origin_{0.0f};
			glm::vec3 grid_size_{256.0f, 256.0f, 128.0f}; // Default size in meters
			float     voxel_size_ = 1.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
