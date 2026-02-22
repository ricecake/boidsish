#include "post_processing/effects/AutoExposureEffect.h"

#include "constants.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		AutoExposureEffect::AutoExposureEffect() {
			name_ = "AutoExposure";
		}

		AutoExposureEffect::~AutoExposureEffect() {
			if (exposureSsbo_) {
				glDeleteBuffers(1, &exposureSsbo_);
			}
		}

		void AutoExposureEffect::Initialize(int /* width */, int /* height */) {
			computeShader_ = std::make_unique<ComputeShader>("shaders/post_processing/auto_exposure.comp");
			passthroughShader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");

			// Create SSBO for adapted luminance, target luminance, limits, and enabled flag
			struct ExposureData {
				float adaptedLuminance;
				float targetLuminance;
				float minExposure;
				float maxExposure;
				int   useAutoExposure;
			};

			glGenBuffers(1, &exposureSsbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, exposureSsbo_);
			ExposureData initialData = {0.3f, targetLuminance_, minExposure_, maxExposure_, 1};
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ExposureData), &initialData, GL_DYNAMIC_DRAW);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), exposureSsbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		void AutoExposureEffect::Apply(
			GLuint sourceTexture,
			GLuint /* depthTexture */,
			GLuint /* velocityTexture */,
			const glm::mat4& /* viewMatrix */,
			const glm::mat4& /* projectionMatrix */,
			const glm::vec3& /* cameraPos */
		) {
			// Update SSBO with latest parameters
			struct ExposureData {
				float adaptedLuminance;
				float targetLuminance;
				float minExposure;
				float maxExposure;
				int   useAutoExposure;
			};

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, exposureSsbo_);
			float actualTarget = targetLuminance_ * (1.0f - nightFactor_ * 0.5f);
			float actualMax = maxExposure_ * (1.0f - nightFactor_ * 0.4f);

			glBufferSubData(
				GL_SHADER_STORAGE_BUFFER,
				offsetof(ExposureData, targetLuminance),
				sizeof(float),
				&actualTarget
			);
			glBufferSubData(
				GL_SHADER_STORAGE_BUFFER,
				offsetof(ExposureData, minExposure),
				sizeof(float),
				&minExposure_
			);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetof(ExposureData, maxExposure), sizeof(float), &actualMax);
			int enabled = is_enabled_ ? 1 : 0;
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetof(ExposureData, useAutoExposure), sizeof(int), &enabled);

			computeShader_->use();
			computeShader_->setFloat("deltaTime", deltaTime_);
			computeShader_->setFloat("speedUp", speedUp_);
			computeShader_->setFloat("speedDown", speedDown_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			computeShader_->setInt("sceneTexture", 0);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), exposureSsbo_);

			glDispatchCompute(1, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			// Passthrough blit
			passthroughShader_->use();
			passthroughShader_->setInt("sceneTexture", 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void AutoExposureEffect::Resize(int /* width */, int /* height */) {
			// Nothing to do for resize
		}

		void AutoExposureEffect::SetEnabled(bool enabled) {
			IPostProcessingEffect::SetEnabled(enabled);

			// Update SSBO immediately if possible (assuming we have context)
			// In this project, UI usually runs on main thread with context.
			if (exposureSsbo_) {
				struct ExposureData {
					float adaptedLuminance;
					float targetLuminance;
					float minExposure;
					float maxExposure;
					int   useAutoExposure;
				};

				glBindBuffer(GL_SHADER_STORAGE_BUFFER, exposureSsbo_);
				int enabledInt = enabled ? 1 : 0;
				glBufferSubData(
					GL_SHADER_STORAGE_BUFFER,
					offsetof(ExposureData, useAutoExposure),
					sizeof(int),
					&enabledInt
				);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}
		}

		// Override SetTime to calculate delta time
		void AutoExposureEffect::SetTime(float time) {
			if (lastTime_ > 0.0f) {
				deltaTime_ = time - lastTime_;
			}
			lastTime_ = time;
		}

	} // namespace PostProcessing
} // namespace Boidsish
