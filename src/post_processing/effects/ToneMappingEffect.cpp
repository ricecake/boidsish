#include "post_processing/effects/ToneMappingEffect.h"

#include "constants.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		ToneMappingEffect::ToneMappingEffect() {
			name_ = "ToneMapping";
		}

		ToneMappingEffect::~ToneMappingEffect() {}

		void ToneMappingEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			_shader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/tonemapping.frag");
		}

		void ToneMappingEffect::Apply(const PostProcessingParams& params) {
			if (!_shader) return;
			_shader->use();
			_shader->setInt("sceneTexture", 0);
			_shader->setInt("toneMode", toneMode_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);

			// Bind AutoExposure SSBO if available
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), 11);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void ToneMappingEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
