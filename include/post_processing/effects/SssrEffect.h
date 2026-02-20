#pragma once

#include <memory>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"
#include "post_processing/TemporalAccumulator.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class SssrEffect: public IPostProcessingEffect {
		public:
			SssrEffect();
			~SssrEffect();

			void Initialize(int width, int height) override;
			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           velocityTexture,
				GLuint           normalTexture,
				GLuint           materialTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;

			bool IsEarly() const override { return true; }

			GLuint GetResultTexture() const override { return temporal_accumulator_.GetResult(); }

			// Parameters
			void  SetIntensity(float intensity) { intensity_ = intensity; }
			float GetIntensity() const { return intensity_; }

			void  SetMaxSteps(int steps) { max_steps_ = steps; }
			int   GetMaxSteps() const { return max_steps_; }

			void  SetRoughnessThreshold(float threshold) { roughness_threshold_ = threshold; }
			float GetRoughnessThreshold() const { return roughness_threshold_; }

		private:
			void InitializeFBOs();
			void GenerateHiZ(GLuint depthTexture);

			int width_ = 0;
			int height_ = 0;

			std::unique_ptr<ComputeShader> hi_z_shader_;
			std::unique_ptr<ComputeShader> sssr_shader_;
			std::unique_ptr<ComputeShader> spatial_filter_shader_;
			std::unique_ptr<Shader>        composite_shader_;

			GLuint hi_z_texture_ = 0;
			int    hi_z_levels_ = 0;
			GLuint spd_counter_buffer_ = 0;

			GLuint trace_texture_ = 0;
			GLuint filter_texture_ = 0;

			TemporalAccumulator temporal_accumulator_;

			float intensity_ = 1.0f;
			int   max_steps_ = 64;
			float roughness_threshold_ = 0.8f;

			uint32_t frame_count_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
