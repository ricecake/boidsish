#include "post_processing/effects/SssEffect.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		SssEffect::SssEffect() {
			name_ = "ScreenSpaceShadows";
			is_enabled_ = true;
		}

		SssEffect::~SssEffect() {}

		void SssEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			sss_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/sss.frag");
		}

		void SssEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint /*velocityTexture*/,
							  const glm::mat4& /*viewMatrix*/, const glm::mat4& /*projectionMatrix*/,
							  const glm::vec3& /*cameraPos*/) {
			if (!sss_shader_ || !sss_shader_->ID) return;

			sss_shader_->use();
			sss_shader_->setInt("sceneTexture", 0);
			sss_shader_->setInt("depthTexture", 1);
			sss_shader_->setInt("sssTileMask", 2);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D_ARRAY, tile_mask_array_);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SssEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	}
}
