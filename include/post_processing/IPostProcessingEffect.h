#pragma once

#include <memory>
#include <string>

#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {
	namespace PostProcessing {

		class IPostProcessingEffect {
		public:
			virtual ~IPostProcessingEffect() = default;

			virtual void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           velocityTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) = 0;
			virtual void Initialize(int width, int height) = 0;
			virtual void Resize(int width, int height) = 0;

			const std::string& GetName() const { return name_; }

			bool IsEnabled() const { return is_enabled_; }

			virtual void SetEnabled(bool enabled) { is_enabled_ = enabled; }

			virtual void SetTime(float /* time */) {}

			virtual void SetNightFactor(float /* factor */) {}

			virtual bool IsEarly() const { return false; }

			virtual float GetRenderScale() const { return render_scale_; }

			virtual void SetRenderScale(float scale) { render_scale_ = scale; }

		protected:
			std::string name_;
			bool        is_enabled_ = true;
			float       render_scale_ = 1.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
