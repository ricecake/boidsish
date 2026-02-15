#include "post_processing/effects/AutoExposureEffect.h"

#include "constants.h"
#include <GL/glew.h>
#include <shader.h>

namespace Boidsish {
	namespace PostProcessing {

		AutoExposureEffect::AutoExposureEffect() {
			name_ = "AutoExposure";
		}

		AutoExposureEffect::~AutoExposureEffect() {
			if (exposureSsbo_ != 0) {
				glDeleteBuffers(1, &exposureSsbo_);
			}
		}

		void AutoExposureEffect::Initialize(int /*width*/, int /*height*/) {
			computeShader_ = std::make_unique<ComputeShader>("shaders/post_processing/auto_exposure.comp");

			// Create SSBO for luminance data
			glGenBuffers(1, &exposureSsbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, exposureSsbo_);
			float initial_data[3] = {0.5f, 0.5f, 1.0f}; // adapted, target, useAutoExposure
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(initial_data), initial_data, GL_DYNAMIC_COPY);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), exposureSsbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		void AutoExposureEffect::Apply(const PostProcessingParams& params) {
			if (!computeShader_ || !computeShader_->isValid())
				return;

			computeShader_->use();
			computeShader_->setInt("sceneTexture", 0);
			computeShader_->setFloat("deltaTime", 0.016f); // TODO: pass actual delta time

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), exposureSsbo_);

			// Dispatch compute shader (work groups of 16x16)
			// We don't need to process every pixel for luminance, so we can use a smaller grid
			computeShader_->dispatch(1, 1, 1);

			// Memory barrier to ensure compute shader writes are visible
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		void AutoExposureEffect::Resize(int /*width*/, int /*height*/) {}

		void AutoExposureEffect::SetEnabled(bool enabled) {
			is_enabled_ = enabled;
			if (exposureSsbo_ != 0) {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, exposureSsbo_);
				float val = enabled ? 1.0f : 0.0f;
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(float), sizeof(float), &val);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}
		}

		void AutoExposureEffect::SetTime(float time) {
			if (lastTime_ > 0.0f) {
				deltaTime_ = time - lastTime_;
			}
			lastTime_ = time;
		}

	} // namespace PostProcessing
} // namespace Boidsish
