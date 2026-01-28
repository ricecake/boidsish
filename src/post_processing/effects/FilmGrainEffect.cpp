#include "post_processing/effects/FilmGrainEffect.h"

#include "shader.h"
#include <GLFW/glfw3.h>

namespace Boidsish {
	namespace PostProcessing {

		FilmGrainEffect::FilmGrainEffect(): intensity_(0.5f) {
			name_ = "Film Grain";
			is_enabled_ = false; // Disabled by default
		}

		void FilmGrainEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/post_processing/film_grain.frag");
			shader_->use();
			shader_->setInt("screenTexture", 0);
		}

		void FilmGrainEffect::Apply(GLuint sourceTexture, GLuint depthTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			shader_->use();
			shader_->setFloat("intensity", intensity_);
			shader_->setFloat("time", static_cast<float>(glfwGetTime()));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void FilmGrainEffect::Resize(int width, int height) {
			// This effect does not need to respond to resize events.
		}

	} // namespace PostProcessing
} // namespace Boidsish
