#include "post_processing/effects/GlitchEffect.h"

#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		GlitchEffect::GlitchEffect() {
			name_ = "Glitch";
		}

		GlitchEffect::~GlitchEffect() {}

		void GlitchEffect::Initialize(int /*width*/, int /*height*/) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/glitch.frag");
		}

		void GlitchEffect::Apply(
			GLuint sourceTexture,
			GLuint depthTexture,
			GLuint /* velocityTexture */, GLuint normalTexture, GLuint materialTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setFloat("time", time_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void GlitchEffect::Resize(int /*width*/, int /*height*/) {
			// No specific resizing needed for this effect
		}

	} // namespace PostProcessing
} // namespace Boidsish
