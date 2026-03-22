#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class VoxelVolumeEffect : public IPostProcessingEffect {
		public:
			VoxelVolumeEffect();
			virtual ~VoxelVolumeEffect();

			void Initialize(int width, int height) override;
			void Apply(
				GLuint sourceTexture,
				GLuint depthTexture,
				GLuint velocityTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;
			void SetTime(float time) override { time_ = time; }
			void SetBlueNoiseTexture(GLuint tex) { blue_noise_texture_ = tex; }

			bool IsEarly() const override { return false; }

		private:
			int width_, height_;
			std::unique_ptr<Shader> shader_;
			float time_ = 0.0f;
			GLuint blue_noise_texture_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
