#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class SdfVolumeEffect: public IPostProcessingEffect {
		public:
			SdfVolumeEffect();
			virtual ~SdfVolumeEffect();

			void Initialize(int width, int height) override;
			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           velocityTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

			void SetNoiseTextures(GLuint noise, GLuint curl) {
				noise_texture_ = noise;
				curl_noise_texture_ = curl;
			}

			bool IsEarly() const override { return true; }

			Shader* GetShader() { return shader_.get(); }

		private:
			std::unique_ptr<Shader> shader_;
			GLuint                  noise_texture_ = 0;
			GLuint                  curl_noise_texture_ = 0;
			int                     width_;
			int                     height_;
			float                   time_ = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
