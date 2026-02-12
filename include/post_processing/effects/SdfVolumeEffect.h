#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class SdfVolumeEffect: public IPostProcessingEffect {
		public:
			SdfVolumeEffect();
			virtual ~SdfVolumeEffect();

			void Initialize(int width, int height) override;
			void Apply(const PostProcessingParams& params) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

		private:
			std::unique_ptr<Shader> shader_;
			int                     width_;
			int                     height_;
			float                   time_ = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
