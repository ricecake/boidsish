#pragma once

#include <memory>
#include "post_processing/IPostProcessingEffect.h"

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class DetailEnhancementEffect : public IPostProcessingEffect {
		public:
			DetailEnhancementEffect();
			~DetailEnhancementEffect();

			void Initialize(int width, int height) override;
			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           velocityTexture,
				GLuint           normalTexture,
				GLuint           albedoTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;

			// Getters and Setters for Parameters
			float GetStrength() const { return strength_; }
			void SetStrength(float s) { strength_ = s; }

			float GetStrengthExponent() const { return strength_exponent_; }
			void SetStrengthExponent(float e) { strength_exponent_ = e; }

			float GetStrengthScale() const { return strength_scale_; }
			void SetStrengthScale(float s) { strength_scale_ = s; }

			float GetStrengthBias() const { return strength_bias_; }
			void SetStrengthBias(float b) { strength_bias_ = b; }

			float GetEpsilonBase() const { return epsilon_base_; }
			void SetEpsilonBase(float eb) { epsilon_base_ = eb; }

			float GetEpsilonExponent() const { return epsilon_exponent_; }
			void SetEpsilonExponent(float ee) { epsilon_exponent_ = ee; }

			float GetEpsilonScale() const { return epsilon_scale_; }
			void SetEpsilonScale(float es) { epsilon_scale_ = es; }

			float GetEpsilonBias() const { return epsilon_bias_; }
			void SetEpsilonBias(float eb) { epsilon_bias_ = eb; }

		private:
			void InitializeResources();

			std::unique_ptr<Shader> shader_;
			GLuint                  mip_texture_;
			int                     width_;
			int                     height_;

			// Master strength
			float strength_;

			// Detail Boost Curve parameters
			float strength_exponent_;
			float strength_scale_;
			float strength_bias_;

			// Guiding Strength (Epsilon) Curve parameters
			float epsilon_base_;
			float epsilon_exponent_;
			float epsilon_scale_;
			float epsilon_bias_;
		};

	} // namespace PostProcessing
} // namespace Boidsish
