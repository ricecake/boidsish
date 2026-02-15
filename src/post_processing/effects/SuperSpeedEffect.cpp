#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/SuperSpeedEffect.h"

#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		SuperSpeedEffect::SuperSpeedEffect() {
			name_ = "SuperSpeed";
			is_enabled_ = true; // Default to true, but intensity will be 0
		}

		SuperSpeedEffect::~SuperSpeedEffect() {}

		void SuperSpeedEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/super_speed.frag");
			width_ = width;
			height_ = height;
		}

		void SuperSpeedEffect::Apply(
			GLuint           sourceTexture,
			const GBuffer&   gbuffer,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setFloat("time", time_);
			shader_->setFloat("intensity", intensity_);
			shader_->setVec2("center", glm::vec2(0.5f, 0.5f)); // Focus on center of screen

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SuperSpeedEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
