#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class GlitchEffect: public IPostProcessingEffect {
		public:
			GlitchEffect();
			~GlitchEffect();

			void Apply(GLuint sourceTexture) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			void SetTime(float time) { time_ = time; }

		private:
			std::unique_ptr<Shader> shader_;
			float                   time_ = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
