#include "post_processing/effects/SssrEffect.h"
#include <GL/glew.h>

namespace Boidsish::PostProcessing {

	SssrEffect::SssrEffect() {
		name_ = "SSSR";
	}

	SssrEffect::~SssrEffect() = default;

	void SssrEffect::Initialize(int width, int height) {
		width_ = width;
		height_ = height;
		sssr_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/sssr.frag");
	}

	void SssrEffect::Resize(int width, int height) {
		width_ = width;
		height_ = height;
	}

	void SssrEffect::Apply(const PostProcessingParams& params) {
		if (!sssr_shader_)
			return;

		sssr_shader_->use();

		// Bind textures
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
		sssr_shader_->setInt("sceneTexture", 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, params.depthTexture);
		sssr_shader_->setInt("depthTexture", 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, params.normalTexture);
		sssr_shader_->setInt("normalTexture", 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, params.pbrTexture);
		sssr_shader_->setInt("pbrTexture", 3);

		glActiveTexture(GL_TEXTURE8); // Move Hi-Z to Unit 8 to avoid conflict with shadows (Unit 4)
		glBindTexture(GL_TEXTURE_2D, params.hizTexture);
		sssr_shader_->setInt("hizTexture", 8);

		// Uniforms
		sssr_shader_->setMat4("projection", params.projectionMatrix);
		sssr_shader_->setMat4("view", params.viewMatrix);
		sssr_shader_->setMat4("invProjection", params.invProjectionMatrix);
		sssr_shader_->setMat4("invView", params.invViewMatrix);
		sssr_shader_->setVec3("cameraPos", params.cameraPos);
		sssr_shader_->setFloat("time", params.time);
		sssr_shader_->setUint("frameCount", params.frameCount);
		sssr_shader_->setFloat("worldScale", params.worldScale);

		// Render full-screen quad - VAO is already bound by PostProcessingManager
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

} // namespace Boidsish::PostProcessing
