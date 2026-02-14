#include "post_processing/effects/GlitchEffect.h"

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		GlitchEffect::GlitchEffect() {
			name_ = "Glitch";
		}

		GlitchEffect::~GlitchEffect() {}

		void GlitchEffect::Initialize(int /*width*/, int /*height*/) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/glitch.frag");
		}

		void GlitchEffect::Apply(const PostProcessingParams& params) {
			if (!shader_) return;
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setFloat("time", time_);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void GlitchEffect::Resize(int /*width*/, int /*height*/) {}

	} // namespace PostProcessing
} // namespace Boidsish
