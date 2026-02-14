#include "post_processing/effects/SuperSpeedEffect.h"

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		SuperSpeedEffect::SuperSpeedEffect() {
			name_ = "SuperSpeed";
		}

		SuperSpeedEffect::~SuperSpeedEffect() = default;

		void SuperSpeedEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/super_speed.frag");
		}

		void SuperSpeedEffect::Apply(const PostProcessingParams& params) {
			if (!shader_) return;
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setFloat("time", time_);
			shader_->setFloat("intensity", intensity_);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SuperSpeedEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
