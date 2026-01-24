#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"
#include "shader.h"
#include <glm/glm.hpp>
#include <vector>

namespace Boidsish {
	namespace PostProcessing {

		class ToneMappingEffect: public IPostProcessingEffect {
		public:
			ToneMappingEffect();
			~ToneMappingEffect() override;

			void Initialize(int width, int height) override;
			void Apply(GLuint sourceTexture, float delta_time) override;
			void Resize(int width, int height) override;

			void SetMode(float toneMode) { toneMode_ = toneMode; }

			int GetMode() const { return toneMode_; }

			void SetAdaptationSpeedUp(float speed) { _adaptationSpeedUp = speed; }
			void SetAdaptationSpeedDown(float speed) { _adaptationSpeedDown = speed; }
			void SetTargetLuminance(float luminance) { _targetLuminance = luminance; }
			void SetExposureClamp(const glm::vec2& clamp) { _exposureClamp = clamp; }

		private:
			std::unique_ptr<Shader> _shader;
			std::unique_ptr<Shader> _downsampleShader;
			std::unique_ptr<Shader> _adaptationShader;
			std::vector<GLuint>     _mipChainFBO;
			std::vector<GLuint>     _mipChainTexture;
			GLuint                  _lumPingPongFBO[2];
			GLuint                  _lumPingPongTexture[2];
			int                     _lumPingPongIndex = 0;
			float                   _adaptationSpeedUp = 0.1f;
			float                   _adaptationSpeedDown = 0.05f;
			float                   _targetLuminance = 0.5f;
			glm::vec2               _exposureClamp = glm::vec2(0.1f, 10.0f);

			int                     width_ = 0;
			int                     height_ = 0;
			int                     toneMode_ = 2;
		};

	} // namespace PostProcessing
} // namespace Boidsish
