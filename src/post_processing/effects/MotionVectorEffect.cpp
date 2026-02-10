#include "post_processing/effects/MotionVectorEffect.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		MotionVectorEffect::MotionVectorEffect() {
			name_ = "MotionVector";
		}

		MotionVectorEffect::~MotionVectorEffect() = default;

		void MotionVectorEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/motion_vectors.frag");
		}

		void MotionVectorEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

		void MotionVectorEffect::Apply(const PostProcessingParams& params) {
			shader_->use();
			shader_->setInt("depthTexture", 0);
			shader_->setMat4("invViewProj", glm::inverse(params.projectionMatrix * params.viewMatrix));
			shader_->setMat4("prevViewProj", params.prevProjectionMatrix * params.prevViewMatrix);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.depthTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

	} // namespace PostProcessing
} // namespace Boidsish
