#include "post_processing/effects/DetailEnhancementEffect.h"
#include "shader.h"
#include <iostream>

namespace Boidsish {
	namespace PostProcessing {

		DetailEnhancementEffect::DetailEnhancementEffect() :
			mip_texture_(0),
			width_(1),
			height_(1),
			strength_(0.5f),
			strength_exponent_(-0.5f),
			strength_scale_(1.0f),
			strength_bias_(0.5f),
			epsilon_base_(0.01f),
			epsilon_exponent_(1.0f),
			epsilon_scale_(1.0f),
			epsilon_bias_(0.0f) {
			name_ = "Detail Enhancement";
			is_enabled_ = false; // Disabled by default
		}

		DetailEnhancementEffect::~DetailEnhancementEffect() {
			if (mip_texture_ != 0) {
				glDeleteTextures(1, &mip_texture_);
			}
		}

		void DetailEnhancementEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/post_processing/detail_enhancement.frag"
			);

			InitializeResources();
		}

		void DetailEnhancementEffect::InitializeResources() {
			if (mip_texture_ != 0) {
				glDeleteTextures(1, &mip_texture_);
				mip_texture_ = 0;
			}

			glGenTextures(1, &mip_texture_);
			glBindTexture(GL_TEXTURE_2D, mip_texture_);
			// We allocate 5 mipmap levels
			glTexStorage2D(GL_TEXTURE_2D, 5, GL_RGBA16F, width_, height_);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void DetailEnhancementEffect::Apply(
			GLuint           sourceTexture,
			GLuint           /* depthTexture */,
			GLuint           /* velocityTexture */,
			GLuint           /* normalTexture */,
			GLuint           /* albedoTexture */,
			const glm::mat4& /* viewMatrix */,
			const glm::mat4& /* projectionMatrix */,
			const glm::vec3& /* cameraPos */
		) {
			if (mip_texture_ == 0)
				return;

			// Copy the sourceTexture into Level 0 of our mip_texture_
			glCopyImageSubData(
				sourceTexture, GL_TEXTURE_2D, 0, 0, 0, 0,
				mip_texture_, GL_TEXTURE_2D, 0, 0, 0, 0,
				width_, height_, 1
			);

			// Generate mipmaps for our texture
			glBindTexture(GL_TEXTURE_2D, mip_texture_);
			glGenerateMipmap(GL_TEXTURE_2D);

			// Setup and dispatch the detail enhancement shader
			shader_->use();
			shader_->setInt("screenTexture", 0);
			shader_->setVec2("uResolution", static_cast<float>(width_), static_cast<float>(height_));

			// Set effect strength & curve parameters
			shader_->setFloat("uStrength", strength_);
			shader_->setFloat("uStrengthExponent", strength_exponent_);
			shader_->setFloat("uStrengthScale", strength_scale_);
			shader_->setFloat("uStrengthBias", strength_bias_);

			shader_->setFloat("uEpsilonBase", epsilon_base_);
			shader_->setFloat("uEpsilonExponent", epsilon_exponent_);
			shader_->setFloat("uEpsilonScale", epsilon_scale_);
			shader_->setFloat("uEpsilonBias", epsilon_bias_);

			// Bind mip texture
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mip_texture_);

			// Render full-screen quad
			glDrawArrays(GL_TRIANGLES, 0, 6);

			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void DetailEnhancementEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeResources();
		}

	} // namespace PostProcessing
} // namespace Boidsish
