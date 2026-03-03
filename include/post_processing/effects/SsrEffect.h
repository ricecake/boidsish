#pragma once

#include <memory>
#include "post_processing/IPostProcessingEffect.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class SsrEffect : public IPostProcessingEffect {
		public:
			SsrEffect();
			~SsrEffect();

			void Initialize(int width, int height) override;
			void Apply(const PostProcessingContext& context) override;
			void Resize(int width, int height) override;

			bool IsEarly() const override { return true; }

			void SetEnabled(bool enabled) override { is_enabled_ = enabled; }

			// Parameters
			void SetIntensity(float i) { intensity_ = i; }
			float GetIntensity() const { return intensity_; }
			void SetMaxDistance(float d) { max_distance_ = d; }
			float GetMaxDistance() const { return max_distance_; }
			void SetStride(float s) { stride_ = s; }
			float GetStride() const { return stride_; }
			void SetThickness(float t) { thickness_ = t; }
			float GetThickness() const { return thickness_; }

		private:
			void CreateFBOs(int width, int height);

			std::unique_ptr<ComputeShader> ssr_shader_;
			std::unique_ptr<Shader> composite_shader_;
			std::unique_ptr<Shader> passthrough_shader_;

			GLuint ssr_texture_ = 0;
			GLuint temporal_texture_ = 0;
			GLuint temporal_fbo_ = 0;

			int width_ = 0;
			int height_ = 0;

			float intensity_ = 1.0f;
			float max_distance_ = 100.0f;
			float stride_ = 2.0f;
			float thickness_ = 0.5f;
		};

	}
}
