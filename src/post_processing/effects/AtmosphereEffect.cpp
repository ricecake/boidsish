#include "post_processing/effects/AtmosphereEffect.h"

#include "ConfigManager.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		AtmosphereEffect::AtmosphereEffect() {
			name_ = "Atmosphere";
		}

		AtmosphereEffect::~AtmosphereEffect() {}

		void AtmosphereEffect::Initialize(int /*width*/, int /*height*/) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/atmosphere.frag");
		}

		void AtmosphereEffect::Apply(const PostProcessingParams& params) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setMat4("invProjection", params.invProjectionMatrix);
			shader_->setMat4("invView", params.invViewMatrix);
			shader_->setVec3("viewPos", params.cameraPos);

			auto& config = ConfigManager::GetInstance();
			shader_->setBool("enableClouds", config.GetAppSettingBool("enable_clouds", true));
			shader_->setBool("enableFog", config.GetAppSettingBool("enable_fog", true));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, params.depthTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void AtmosphereEffect::Resize(int /*width*/, int /*height*/) {}

	} // namespace PostProcessing
} // namespace Boidsish
