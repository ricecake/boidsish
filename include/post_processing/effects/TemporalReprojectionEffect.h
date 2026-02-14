#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <memory>

class Shader;

namespace Boidsish {
	namespace PostProcessing {

		class TemporalReprojectionEffect: public IPostProcessingEffect {
		public:
			TemporalReprojectionEffect();
			~TemporalReprojectionEffect();

			void Apply(const PostProcessingParams& params) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

		private:
			void InitializeFBOs();

			std::unique_ptr<Shader> shader_;
			int                     width_, height_;
			GLuint                  history_fbo_[2];
			GLuint                  history_texture_[2];
			int                     current_read_ = 0;
			bool                    first_frame_ = true;
		};

	} // namespace PostProcessing
} // namespace Boidsish
