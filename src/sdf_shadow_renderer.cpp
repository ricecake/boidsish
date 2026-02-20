#include "sdf_shadow_renderer.h"

#include "ConfigManager.h"
#include "decor_manager.h"
#include "logger.h"
#include "model.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	SdfShadowRenderer::SdfShadowRenderer() {}

	SdfShadowRenderer::~SdfShadowRenderer() {
		if (fbo_)
			glDeleteFramebuffers(1, &fbo_);
		if (shadow_texture_)
			glDeleteTextures(1, &shadow_texture_);
		if (box_vao_) {
			glDeleteVertexArrays(1, &box_vao_);
			glDeleteBuffers(1, &box_vbo_);
			glDeleteBuffers(1, &box_ebo_);
		}
	}

	void SdfShadowRenderer::Initialize(int width, int height) {
		width_ = width;
		height_ = height;

		collect_shader_ = std::make_unique<Shader>("shaders/sdf_shadow_collect.vert", "shaders/sdf_shadow_collect.frag");

		glGenFramebuffers(1, &fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

		glGenTextures(1, &shadow_texture_);
		glBindTexture(GL_TEXTURE_2D, shadow_texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shadow_texture_, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			logger::ERROR("SdfShadowRenderer FBO incomplete");
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		_SetupBoxGeometry();
	}

	void SdfShadowRenderer::Resize(int width, int height) {
		width_ = width;
		height_ = height;
		glBindTexture(GL_TEXTURE_2D, shadow_texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	}

	void SdfShadowRenderer::_SetupBoxGeometry() {
		float vertices[] = {
			-0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
			-0.5f, -0.5f, 0.5f,  0.5f, -0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  -0.5f, 0.5f, 0.5f
		};

		unsigned int indices[] = {
			0, 1, 3, 3, 1, 2, 1, 5, 2, 2, 5, 6, 5, 4, 6, 6, 4, 7,
			4, 0, 7, 7, 0, 3, 3, 2, 7, 7, 2, 6, 4, 5, 0, 0, 5, 1
		};

		glGenVertexArrays(1, &box_vao_);
		glGenBuffers(1, &box_vbo_);
		glGenBuffers(1, &box_ebo_);

		glBindVertexArray(box_vao_);
		glBindBuffer(GL_ARRAY_BUFFER, box_vbo_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, box_ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

		glBindVertexArray(0);
	}

	void SdfShadowRenderer::RenderShadows(
		const std::vector<std::shared_ptr<Shape>>& shapes,
		DecorManager*                              decor_manager,
		GLuint                                     depth_texture,
		GLuint                                     hiz_texture,
		const glm::mat4&                           view,
		const glm::mat4&                           projection,
		const glm::vec3&                           light_dir
	) {
		if (!collect_shader_ || !collect_shader_->isValid())
			return;

		auto& cfg = ConfigManager::GetInstance();
		if (!cfg.GetAppSettingBool("enable_sdf_shadows", true)) {
			glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
			glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glViewport(0, 0, width_, height_);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glEnable(GL_BLEND);
		glBlendEquation(GL_MIN);
		glBlendFunc(GL_ONE, GL_ONE); // Factors are ignored by GL_MIN
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);

		collect_shader_->use();
		collect_shader_->setMat4("view", view);
		collect_shader_->setMat4("projection", projection);
		collect_shader_->setVec3("u_worldLightDir", light_dir);
		collect_shader_->setFloat("u_sdfShadowSoftness", cfg.GetAppSettingFloat("sdf_shadow_softness", 10.0f));
		collect_shader_->setFloat("u_sdfShadowMaxDist", cfg.GetAppSettingFloat("sdf_shadow_max_dist", 2.0f));
		collect_shader_->setFloat("u_sdfShadowBias", cfg.GetAppSettingFloat("sdf_shadow_bias", 0.05f));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depth_texture);
		collect_shader_->setInt("u_depthTexture", 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, hiz_texture);
		collect_shader_->setInt("u_hizTexture", 1);

		glBindVertexArray(box_vao_);

		// Render shapes
		for (const auto& shape : shapes) {
			auto model_shape = std::dynamic_pointer_cast<Model>(shape);
			if (!model_shape)
				continue;

			const auto& model_data = model_shape->GetModelData();
			if (!model_data || model_data->sdf_texture == 0)
				continue;

			// Calculate a bounding box that covers the model and its shadow
			// For now, just use the model's AABB scaled up a bit
			glm::mat4 model_mat = model_shape->GetModelMatrix();
			glm::mat4 inv_model = glm::inverse(model_mat);

			collect_shader_->setMat4("model", model_mat);
			collect_shader_->setMat4("invModel", inv_model);
			collect_shader_->setBool("is_instanced", false);
			collect_shader_->setBool("useSSBOInstancing", false);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_3D, model_data->sdf_texture);
			collect_shader_->setInt("u_sdfTexture", 2);
			collect_shader_->setVec3("u_sdfExtent", model_data->sdf_extent);
			collect_shader_->setVec3("u_sdfMin", model_data->sdf_min);

			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
		}

		// Render decor
		if (decor_manager) {
			const auto& types = decor_manager->GetDecorTypes();
			for (size_t i = 0; i < types.size(); ++i) {
				const auto& type = types[i];
				const auto& model_data = type.model->GetModelData();
				if (!model_data || model_data->sdf_texture == 0 || type.cached_count == 0)
					continue;

				collect_shader_->setBool("useSSBOInstancing", true);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, type.ssbo);

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_3D, model_data->sdf_texture);
				collect_shader_->setInt("u_sdfTexture", 2);
				collect_shader_->setVec3("u_sdfExtent", model_data->sdf_extent);
				collect_shader_->setVec3("u_sdfMin", model_data->sdf_min);

				// For decor, we'd need to render the box for each instance.
				// This is expensive if we do it in a loop.
				// Instead, we should use instanced rendering of the box!

				// We need the instances' model matrices. They are in the SSBO.
				// But we want to render BOXES, not the model meshes.
				// We can use the same SSBO but modify the vertex shader to scale the box.

				// TODO: Optimize decor SDF shadow rendering
				// For now, let's just do a few or skip if too many.
				// Actually, let's use the SSBO and just draw instanced boxes.

				glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, type.cached_count);
			}
		}

		glDisable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD); // Restore default
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

} // namespace Boidsish
