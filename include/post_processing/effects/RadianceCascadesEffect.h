#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <memory>
#include <vector>

class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		class RadianceCascadesEffect : public IPostProcessingEffect {
		public:
			RadianceCascadesEffect();
			virtual ~RadianceCascadesEffect();

			virtual void Initialize(int width, int height) override;
			virtual void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           velocityTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			virtual void Resize(int width, int height) override;

			virtual bool IsEarly() const override { return true; }

			float GetIntensity() const { return intensity_; }
			void  SetIntensity(float intensity) { intensity_ = intensity; }

			int  GetMaxSteps() const { return max_steps_; }
			void SetMaxSteps(int steps) { max_steps_ = steps; }

			bool IsHiZEnabled() const { return enable_hiz_; }
			void SetHiZEnabled(bool enabled) { enable_hiz_ = enabled; }

			float GetTemporalAlpha() const { return temporal_alpha_; }
			void  SetTemporalAlpha(float alpha) { temporal_alpha_ = alpha; }

		private:
			void InitializeResources();
			void GenerateHiZ(GLuint depthTexture);

			int width_, height_;
			float intensity_ = 1.0f;
			int   max_steps_ = 64;
			bool  enable_hiz_ = true;
			float temporal_alpha_ = 0.9f;

			std::unique_ptr<ComputeShader> gen_shader_;
			std::unique_ptr<ComputeShader> merge_shader_;
			std::unique_ptr<Shader>        composite_shader_;
			std::unique_ptr<ComputeShader> hiz_shader_;
			std::unique_ptr<ComputeShader> accum_shader_;

			GLuint cascades_texture_ = 0;
			GLuint hiz_texture_ = 0;
			GLuint history_textures_[2] = {0, 0};
			int    current_history_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
