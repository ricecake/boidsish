#pragma once

#include <memory>
#include <string>

#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {
	namespace PostProcessing {

		struct PostProcessingContext {
			GLuint sourceTexture;
			GLuint depthTexture;
			GLuint velocityTexture;
			GLuint normalRoughnessTexture; // G-Buffer
			GLuint albedoMetallicTexture;  // G-Buffer
			GLuint hizTexture;
			GLuint targetFbo;

			glm::mat4 viewMatrix;
			glm::mat4 projectionMatrix;
			glm::mat4 invViewMatrix;
			glm::mat4 invProjectionMatrix;
			glm::vec3 cameraPos;
			float     time;
			int       width;
			int       height;
		};

		class IPostProcessingEffect {
		public:
			virtual ~IPostProcessingEffect() = default;

			virtual void Apply(const PostProcessingContext& context) = 0;
			virtual void Initialize(int width, int height) = 0;
			virtual void Resize(int width, int height) = 0;

			const std::string& GetName() const { return name_; }

			bool IsEnabled() const { return is_enabled_; }

			virtual void SetEnabled(bool enabled) { is_enabled_ = enabled; }

			virtual void SetTime(float /* time */) {}

			virtual void SetNightFactor(float /* factor */) {}

			virtual bool IsEarly() const { return false; }

		protected:
			std::string name_;
			bool        is_enabled_ = true;
		};

	} // namespace PostProcessing
} // namespace Boidsish
