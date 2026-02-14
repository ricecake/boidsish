#include "post_processing/effects/SdfVolumeEffect.h"

#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		SdfVolumeEffect::SdfVolumeEffect() {
			name_ = "SdfVolume";
		}

		SdfVolumeEffect::~SdfVolumeEffect() {}

		void SdfVolumeEffect::Initialize(int /*width*/, int /*height*/) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/sdf_volume.frag");
		}

		void SdfVolumeEffect::Apply(const PostProcessingParams& params) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setMat4("invProjection", params.invProjectionMatrix);
			shader_->setMat4("invView", params.invViewMatrix);
			shader_->setVec3("viewPos", params.cameraPos);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, params.depthTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SdfVolumeEffect::Resize(int /*width*/, int /*height*/) {}

	} // namespace PostProcessing
} // namespace Boidsish
