#include "hiz_manager.h"

#include <algorithm>

#include "service_locator.h"
#include "constants.h"
#include "gpu_resource_registry.h"
#include <cmath>
#include <iostream>

#include "profiler.h"
#include "shader.h"

namespace Boidsish {

	HiZManager::HiZManager(ServiceLocator& /*loc*/) {}

	HiZManager::~HiZManager() {
		DestroyTexture();
		if (empty_vao_) {
			glDeleteVertexArrays(1, &empty_vao_);
			empty_vao_ = 0;
		}
	}

	void HiZManager::Initialize(int width, int height) {
		PROJECT_PROFILE_SCOPE("HiZManager::Initialize");
		render_width_ = width;
		render_height_ = height;
		hiz_width_ = std::max(1, width / 2);
		hiz_height_ = std::max(1, height / 2);

		decode_shader_ = std::make_unique<Shader>("shaders/hiz_decode.vert", "shaders/hiz_decode.frag");
		if (!decode_shader_->isValid()) {
			std::cerr << "HiZManager: Failed to compile hiz_decode shaders" << std::endl;
			return;
		}

		glGenVertexArrays(1, &empty_vao_);

		CreateTexture();
		initialized_ = true;
	}

	void HiZManager::Resize(int width, int height) {
		if (width == render_width_ && height == render_height_)
			return;

		render_width_ = width;
		render_height_ = height;
		hiz_width_ = std::max(1, width / 2);
		hiz_height_ = std::max(1, height / 2);

		DestroyTexture();
		CreateTexture();
	}

	void HiZManager::CreateTexture() {
		mip_count_ = 1 + static_cast<int>(std::floor(std::log2(std::max(hiz_width_, hiz_height_))));

		glGenTextures(1, &hiz_texture_);
		glBindTexture(GL_TEXTURE_2D, hiz_texture_);
		glTexStorage2D(GL_TEXTURE_2D, mip_count_, GL_R32F, hiz_width_, hiz_height_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		GpuResourceRegistry::Instance().PublishTexture(Constants::TextureUnit::HiZ(), hiz_texture_);

		// Create temp FBO
		glGenFramebuffers(1, &temp_fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, temp_fbo_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hiz_texture_, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void HiZManager::DestroyTexture() {
		if (hiz_texture_) {
			glDeleteTextures(1, &hiz_texture_);
			hiz_texture_ = 0;
		}
		if (temp_fbo_) {
			glDeleteFramebuffers(1, &temp_fbo_);
			temp_fbo_ = 0;
		}
	}

	void HiZManager::GeneratePyramid(GLuint depthTexture) {
		PROJECT_PROFILE_SCOPE("HiZManager::GeneratePyramid");
		if (!initialized_ || !decode_shader_->isValid())
			return;

		// Save previous viewport and framebuffer binding
		GLint prev_viewport[4];
		glGetIntegerv(GL_VIEWPORT, prev_viewport);

		GLint prev_draw_fbo = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);

		// Save relevant pipeline states
		GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
		GLboolean scissor_test_enabled = glIsEnabled(GL_SCISSOR_TEST);
		GLboolean stencil_test_enabled = glIsEnabled(GL_STENCIL_TEST);
		GLboolean blend_enabled = glIsEnabled(GL_BLEND);
		GLboolean cull_face_enabled = glIsEnabled(GL_CULL_FACE);

		// Disable all standard fixed-function stages for maximum blit/raster fillrate
		if (depth_test_enabled) glDisable(GL_DEPTH_TEST);
		if (scissor_test_enabled) glDisable(GL_SCISSOR_TEST);
		if (stencil_test_enabled) glDisable(GL_STENCIL_TEST);
		if (blend_enabled) glDisable(GL_BLEND);
		if (cull_face_enabled) glDisable(GL_CULL_FACE);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, temp_fbo_);
		decode_shader_->use();

		glBindVertexArray(empty_vao_);

		for (int mip = 0; mip < mip_count_; ++mip) {
			int dst_w = std::max(1, hiz_width_ >> mip);
			int dst_h = std::max(1, hiz_height_ >> mip);

			// Attach current destination mip level to temp_fbo_
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hiz_texture_, mip);

			glViewport(0, 0, dst_w, dst_h);

			glActiveTexture(GL_TEXTURE0);
			if (mip == 0) {
				glBindTexture(GL_TEXTURE_2D, depthTexture);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
			} else {
				glBindTexture(GL_TEXTURE_2D, hiz_texture_);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, mip - 1);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip - 1);
			}
			decode_shader_->setInt("u_srcDepth", 0);

			glDrawArrays(GL_TRIANGLES, 0, 3);

			// Insert memory barrier so the written mip level is visible as a texture source in the next iteration
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
		}

		glBindVertexArray(0);

		// Restore previous fixed-function pipeline states
		if (depth_test_enabled) glEnable(GL_DEPTH_TEST);
		if (scissor_test_enabled) glEnable(GL_SCISSOR_TEST);
		if (stencil_test_enabled) glEnable(GL_STENCIL_TEST);
		if (blend_enabled) glEnable(GL_BLEND);
		if (cull_face_enabled) glEnable(GL_CULL_FACE);

		// Reset texture parameters of hiz_texture_ so subsequent passes can read all levels normally
		glBindTexture(GL_TEXTURE_2D, hiz_texture_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip_count_ - 1);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Restore previous draw framebuffer and viewport
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
		glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);

		// Ensure all Hi-Z mip data is visible to subsequent texture fetches.
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
	}

} // namespace Boidsish
