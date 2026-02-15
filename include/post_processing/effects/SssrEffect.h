#pragma once

#include "post_processing/GBuffer.h"
#include "post_processing/IPostProcessingEffect.h"
#include <memory>

class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		class SssrEffect: public IPostProcessingEffect {
		public:
			SssrEffect();
			~SssrEffect();

			void Apply(
				GLuint           sourceTexture,
				const GBuffer&   gbuffer,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;

			void Calculate(
				GLuint           sourceTexture,
				const GBuffer&   gbuffer,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix
			);

			void Composite(
				GLuint           sourceTexture,
				const GBuffer&   gbuffer,
				const glm::mat4& projectionMatrix
			);
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			bool IsEarly() const override { return true; }

			void SetMaxSamples(int samples) { max_samples_ = samples; }
			int  GetMaxSamples() const { return max_samples_; }

			void  SetRoughnessThreshold(float threshold) { roughness_threshold_ = threshold; }
			float GetRoughnessThreshold() const { return roughness_threshold_; }

			void  SetMirrorThreshold(float threshold) { mirror_threshold_ = threshold; }
			float GetMirrorThreshold() const { return mirror_threshold_; }

            GLuint GetReflectionRadianceBuffer() const { return reflection_texture_; }

		private:
			void InitializeFBOs();

			std::unique_ptr<ComputeShader> sssr_shader_;
			std::unique_ptr<ComputeShader> spatial_filter_shader_;
			std::unique_ptr<ComputeShader> temporal_accumulation_shader_;
			std::unique_ptr<Shader>        composite_shader_;

			GLuint reflection_texture_ = 0;
			GLuint filtered_reflection_texture_ = 0;
			GLuint history_texture_ = 0;
			int    width_ = 0;
			int    height_ = 0;
			uint32_t frame_count_ = 0;

			int   max_samples_ = 1;
			float roughness_threshold_ = 0.8f;
			float mirror_threshold_ = 0.05f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
