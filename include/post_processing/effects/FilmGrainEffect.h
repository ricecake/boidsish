#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class FilmGrainEffect: public IPostProcessingEffect {
		public:
			FilmGrainEffect();

			void Initialize(int width, int height) override;
			void Apply(GLuint sourceTexture, float delta_time) override;
			void Resize(int width, int height) override;

			void SetIntensity(float intensity) { intensity_ = intensity; }

			float GetIntensity() const { return intensity_; }

		private:
			std::unique_ptr<Shader> shader_;
			float                   intensity_;
		};

	} // namespace PostProcessing
} // namespace Boidsish
