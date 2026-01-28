#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class StrobeEffect: public IPostProcessingEffect {
		public:
			StrobeEffect();
			~StrobeEffect();

			void Apply(GLuint sourceTexture, GLuint depthTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

		private:
			void InitializeFBO(int width, int height);

			std::unique_ptr<Shader> shader_;
			std::unique_ptr<Shader> blit_shader_;
			float                   time_ = 0.0f;
			float                   last_capture_time_ = 0.0f;

			GLuint strobe_fbo_ = 0;
			GLuint strobe_texture_ = 0;

			int width_ = 0;
			int height_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
