#include "post_processing/effects/ArtisticGbufferEffect.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		ArtisticGbufferEffect::ArtisticGbufferEffect() {
			name_ = "Artistic G-Buffer";
			is_enabled_ = false;
		}

		ArtisticGbufferEffect::~ArtisticGbufferEffect() {}

		void ArtisticGbufferEffect::Initialize(int /*width*/, int /*height*/) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/artistic_gbuffer.frag");
		}

		void ArtisticGbufferEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& /*viewMatrix*/, const glm::mat4& /*projectionMatrix*/, const glm::vec3& /*cameraPos*/) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setInt("velocityTexture", 2);
			shader_->setInt("normalTexture", 3);
			shader_->setInt("albedoTexture", 4);
			shader_->setFloat("time", time_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, velocityTexture);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, normalTexture);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, albedoTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// Reset active texture
			glActiveTexture(GL_TEXTURE0);
		}

		void ArtisticGbufferEffect::Resize(int /*width*/, int /*height*/) {
			// No specific resizing needed for this effect
		}

	} // namespace PostProcessing
} // namespace Boidsish
