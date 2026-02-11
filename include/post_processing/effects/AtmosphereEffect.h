#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"
#include "post_processing/effects/AtmosphereScattering.h"

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class AtmosphereEffect: public IPostProcessingEffect {
		public:
			AtmosphereEffect();
			~AtmosphereEffect();

			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

			void SetWorldScale(float scale) { world_scale_ = scale; }

			// Cloud parameters
			void SetCloudColor(const glm::vec3& color) { cloud_color_ = color; }

			glm::vec3 GetCloudColor() const { return cloud_color_; }

			AtmosphereScattering&       GetScattering() { return scattering_; }

			const AtmosphereScattering& GetScattering() const { return scattering_; }

		private:
			std::unique_ptr<Shader> shader_;
			float                   time_ = 0.0f;
			float                   world_scale_ = 1.0f;

			glm::vec3 cloud_color_ = glm::vec3(0.95f, 0.95f, 1.0f);

			AtmosphereScattering scattering_;

			int width_ = 0;
			int height_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
