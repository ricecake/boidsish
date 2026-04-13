#ifndef UNIFIED_SCREEN_SPACE_EFFECT_H
#define UNIFIED_SCREEN_SPACE_EFFECT_H

#include <memory>
#include <string>

#include "post_processing/IPostProcessingEffect.h"
#include "post_processing/TemporalAccumulator.h"
#include <glm/glm.hpp>

class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		enum class ScreenSpaceResolution {
			Full = 1,
			Half = 2,
			Quarter = 4
		};

		class UnifiedScreenSpaceEffect : public IPostProcessingEffect {
		public:
			UnifiedScreenSpaceEffect();
			~UnifiedScreenSpaceEffect();

			void Initialize(int width, int height) override;
			void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void Resize(int width, int height) override;

			bool IsEarly() const override { return true; }

			// Global Toggles
			void SetSSGIEnabled(bool enabled) { ssgi_enabled_ = enabled; }
			bool IsSSGIEnabled() const { return ssgi_enabled_; }
			void SetGTAOEnabled(bool enabled) { gtao_enabled_ = enabled; }
			bool IsGTAOEnabled() const { return gtao_enabled_; }
			void SetSSSEnabled(bool enabled) { sss_enabled_ = enabled; }
			bool IsSSSEnabled() const { return sss_enabled_; }

			// SSGI Parameters
			void  SetSSGIIntensity(float intensity) { ssgi_intensity_ = intensity; }
			float GetSSGIIntensity() const { return ssgi_intensity_; }
			void  SetSSGIRadius(float radius) { ssgi_radius_ = radius; }
			float GetSSGIRadius() const { return ssgi_radius_; }
			void  SetSSGIDistanceFalloff(float falloff) { ssgi_falloff_ = falloff; }
			float GetSSGIDistanceFalloff() const { return ssgi_falloff_; }
			void  SetSSGISteps(int steps) { ssgi_steps_ = steps; }
			int   GetSSGISteps() const { return ssgi_steps_; }
			void  SetSSGIRayCount(int rays) { ssgi_ray_count_ = rays; }
			int   GetSSGIRayCount() const { return ssgi_ray_count_; }
			void  SetSSGIReflectionIntensity(float intensity) { ssgi_reflection_intensity_ = intensity; }
			float GetSSGIReflectionIntensity() const { return ssgi_reflection_intensity_; }
			void  SetSSGIRoughnessFactor(float factor) { ssgi_roughness_factor_ = factor; }
			float GetSSGIRoughnessFactor() const { return ssgi_roughness_factor_; }

			// GTAO Parameters
			void  SetGTAOIntensity(float intensity) { gtao_intensity_ = intensity; }
			float GetGTAOIntensity() const { return gtao_intensity_; }
			void  SetGTAORadius(float radius) { gtao_radius_ = radius; }
			float GetGTAORadius() const { return gtao_radius_; }
			void  SetGTAOFalloff(float falloff) { gtao_falloff_ = falloff; }
			float GetGTAOFalloff() const { return gtao_falloff_; }
			void  SetGTAOSteps(int steps) { gtao_steps_ = steps; }
			int   GetGTAOSteps() const { return gtao_steps_; }
			void  SetGTAODirections(int dirs) { gtao_directions_ = dirs; }
			int   GetGTAODirections() const { return gtao_directions_; }

			// SSS Parameters
			void  SetSSSIntensity(float intensity) { sss_intensity_ = intensity; }
			float GetSSSIntensity() const { return sss_intensity_; }
			void  SetSSSRadius(float radius) { sss_radius_ = radius; }
			float GetSSSRadius() const { return sss_radius_; }
			void  SetSSSBias(float bias) { sss_bias_ = bias; }
			float GetSSSBias() const { return sss_bias_; }
			void  SetSSSSteps(int steps) { sss_steps_ = steps; }
			int   GetSSSSteps() const { return sss_steps_; }

			// Resolution
			void SetResolutionScale(ScreenSpaceResolution scale) {
				if (resolution_scale_ != scale) {
					resolution_scale_ = scale;
					Resize(width_, height_);
				}
			}
			ScreenSpaceResolution GetResolutionScale() const { return resolution_scale_; }

			// Textures
			void SetBlueNoiseTexture(GLuint blueNoise) { blue_noise_texture_ = blueNoise; }
			void SetHiZTexture(GLuint hiz, int mips) { hiz_texture_ = hiz; hiz_mips_ = mips; }

			GLuint GetShadowMaskTexture() const { return sss_accumulator_.GetResult(); }

		private:
			void InitializeTextures();

			std::unique_ptr<ComputeShader> unified_shader_;
			std::unique_ptr<Shader>        composite_shader_;
			TemporalAccumulator            gi_ao_accumulator_;
			TemporalAccumulator            sss_accumulator_;

			GLuint gi_ao_texture_ = 0;
			GLuint sss_texture_ = 0;
			GLuint blue_noise_texture_ = 0;
			GLuint hiz_texture_ = 0;
			int    hiz_mips_ = 0;
			int    width_ = 0, height_ = 0;
			int    internal_width_ = 0, internal_height_ = 0;
			ScreenSpaceResolution resolution_scale_ = ScreenSpaceResolution::Quarter;

			// Toggles
			bool ssgi_enabled_ = true;
			bool gtao_enabled_ = true;
			bool sss_enabled_ = true;

			// SSGI
			float ssgi_intensity_ = 1.0f;
			float ssgi_radius_ = 2.0f;
			float ssgi_falloff_ = 1.0f;
			int   ssgi_steps_ = 8;
			int   ssgi_ray_count_ = 2;
			float ssgi_reflection_intensity_ = 1.0f;
			float ssgi_roughness_factor_ = 1.0f;

			// GTAO
			float gtao_intensity_ = 0.5f;
			float gtao_radius_ = 2.0f;
			float gtao_falloff_ = 1.0f;
			int   gtao_steps_ = 8;
			int   gtao_directions_ = 4;

			// SSS
			float sss_intensity_ = 0.25f;
			float sss_radius_ = 1.25f;
			float sss_bias_ = 0.5f;
			int   sss_steps_ = 8;
		};

	} // namespace PostProcessing
} // namespace Boidsish

#endif // UNIFIED_SCREEN_SPACE_EFFECT_H
