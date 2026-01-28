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
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) = 0;
			virtual void Initialize(int width, int height) = 0;
			virtual void Resize(int width, int height) = 0;

			const std::string& GetName() const { return name_; }

			bool IsEnabled() const { return is_enabled_; }

			void SetEnabled(bool enabled) { is_enabled_ = enabled; }

			virtual void SetTime(float /* time */) {}

		protected:
			std::string name_;
			bool        is_enabled_ = true;
		};

	} // namespace PostProcessing
} // namespace Boidsish
