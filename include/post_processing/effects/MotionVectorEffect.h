#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <memory>

class Shader;

namespace Boidsish {
	namespace PostProcessing {

		class MotionVectorEffect : public IPostProcessingEffect {
		public:
			MotionVectorEffect();
			virtual ~MotionVectorEffect();

			void Apply(const PostProcessingParams& params) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

		private:
			std::unique_ptr<Shader> shader_;
			int                     width_, height_;
		};

	} // namespace PostProcessing
} // namespace Boidsish
