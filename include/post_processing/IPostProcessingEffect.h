#pragma once

#include <memory>
#include <string>

#include <GL/glew.h>

class Shader;

namespace Boidsish {
	namespace PostProcessing {

		class IPostProcessingEffect {
		public:
			virtual ~IPostProcessingEffect() = default;

			virtual void Apply(GLuint sourceTexture) = 0;
			virtual void Initialize(int width, int height) = 0;
			virtual void Resize(int width, int height) = 0;

			const std::string& GetName() const { return name_; }

			bool IsEnabled() const { return is_enabled_; }

			void SetEnabled(bool enabled) { is_enabled_ = enabled; }

			void Toggle() { is_enabled_ = !is_enabled_; }

		protected:
			std::string name_;
			bool        is_enabled_ = true;
			std::unique_ptr<Shader> shader_;
			int width_, height_;
		};

	} // namespace PostProcessing
} // namespace Boidsish
