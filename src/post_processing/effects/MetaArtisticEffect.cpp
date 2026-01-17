#include "post_processing/effects/MetaArtisticEffect.h"

#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		MetaArtisticEffect::MetaArtisticEffect() {
			name_ = "Meta Artistic";
		}

		MetaArtisticEffect::~MetaArtisticEffect() {}

		void MetaArtisticEffect::Initialize(int /*width*/, int /*height*/) {
			shader_ = std::make_unique<Shader>("shaders/effects/meta_artistic.vert", "shaders/effects/meta_artistic.frag");
		}

		void MetaArtisticEffect::Apply(GLuint sourceTexture) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setFloat("time", time_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void MetaArtisticEffect::Resize(int /*width*/, int /*height*/) {
			// No specific resizing needed for this effect
		}

	} // namespace PostProcessing
} // namespace Boidsish
