#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class WhispTrailEffect: public IPostProcessingEffect {
		public:
			WhispTrailEffect();
			~WhispTrailEffect();

			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

		private:
			void InitializeFBOs(int width, int height);

			std::unique_ptr<Shader> trail_shader_;
			std::unique_ptr<Shader> blit_shader_;

			float time_ = 0.0f;

			// Ping-pong buffers for the persistent trail
			GLuint trail_fbo_[2]{0};
			GLuint trail_texture_[2]{0};
			int    current_read_ = 0;

			int width_ = 0;
			int height_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
