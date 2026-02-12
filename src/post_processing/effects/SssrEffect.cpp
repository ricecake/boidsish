#include "post_processing/effects/SssrEffect.h"
#include "ConfigManager.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		SssrEffect::SssrEffect() {
			name_ = "SSSR";
			is_enabled_ = true;
		}

		SssrEffect::~SssrEffect() = default;

		void SssrEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/sssr.frag");
		}

		void SssrEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

		void SssrEffect::Apply(const PostProcessingParams& params) {
			auto& config = ConfigManager::GetInstance();
			is_enabled_ = config.GetAppSettingBool("sssr_enabled", true);
			if (!is_enabled_) {
				return;
			}

			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setInt("normalTexture", 2);
			shader_->setInt("pbrTexture", 3);
			shader_->setInt("hizTexture", 4);
			shader_->setMat4("view", params.viewMatrix);
			shader_->setMat4("projection", params.projectionMatrix);
			shader_->setMat4("invProjection", params.invProjectionMatrix);
			shader_->setMat4("invView", params.invViewMatrix);
			shader_->setFloat("time", params.time);
			shader_->setUint("frameCount", params.frameCount);
			shader_->setVec3("viewPos", params.cameraPos);

			// Load parameters from config
			shader_->setInt("maxSteps", config.GetAppSettingInt("sssr_max_steps", 64));
			shader_->setInt("binarySteps", config.GetAppSettingInt("sssr_binary_steps", 8));
			shader_->setFloat("rayStepSize", config.GetAppSettingFloat("sssr_ray_step_size", 0.5f));
			shader_->setFloat("maxDistance", config.GetAppSettingFloat("sssr_max_distance", 500.0f));
			shader_->setFloat("jitterStrength", config.GetAppSettingFloat("sssr_jitter_strength", 0.2f));
			shader_->setFloat("thicknessBias", config.GetAppSettingFloat("sssr_thickness_bias", 0.1f));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, params.depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, params.normalTexture);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, params.pbrTexture);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, params.hizTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

	} // namespace PostProcessing
} // namespace Boidsish
