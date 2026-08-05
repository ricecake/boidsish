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
		if (src_fbo_) {
			glDeleteFramebuffers(1, &src_fbo_);
			src_fbo_ = 0;
		}
	}

	void HiZManager::Initialize(int width, int height) {
		PROJECT_PROFILE_SCOPE("HiZManager::Initialize");
		render_width_ = width;
		render_height_ = height;
		hiz_width_ = std::max(1, width / 2);
		hiz_height_ = std::max(1, height / 2);

		generate_shader_ = std::make_unique<ComputeShader>("shaders/hiz_generate.comp");
		if (!generate_shader_->isValid()) {
			std::cerr << "HiZManager: Failed to compile hiz_generate.comp" << std::endl;
			return;
		}

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

		// Create temp depth-only texture of full render size
		glGenTextures(1, &temp_depth_texture_);
		glBindTexture(GL_TEXTURE_2D, temp_depth_texture_);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, render_width_, render_height_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Create temp FBO
		glGenFramebuffers(1, &temp_fbo_);
		glBindFramebuffer(GL_FRAMEBUFFER, temp_fbo_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, temp_depth_texture_, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void HiZManager::DestroyTexture() {
		if (hiz_texture_) {
			glDeleteTextures(1, &hiz_texture_);
			hiz_texture_ = 0;
		}
		if (temp_depth_texture_) {
			glDeleteTextures(1, &temp_depth_texture_);
			temp_depth_texture_ = 0;
		}
		if (temp_fbo_) {
			glDeleteFramebuffers(1, &temp_fbo_);
			temp_fbo_ = 0;
		}
	}

	void HiZManager::GeneratePyramid(GLuint depthTexture) {
		PROJECT_PROFILE_SCOPE("HiZManager::GeneratePyramid");
		if (!initialized_ || !generate_shader_->isValid())
			return;

		// Save previous framebuffer bindings to restore them afterwards
		GLint prev_read_fbo = 0;
		GLint prev_draw_fbo = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);

		// Perform hardware-accelerated blit of packed depth-stencil to pure GL_DEPTH_COMPONENT32F
		if (!src_fbo_) {
			glGenFramebuffers(1, &src_fbo_);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, src_fbo_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

		// Disable scissor test during blit to ensure full copy
		GLboolean scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
		if (scissor_enabled) {
			glDisable(GL_SCISSOR_TEST);
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo_);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, temp_fbo_);
		glBlitFramebuffer(
			0, 0, render_width_, render_height_,
			0, 0, render_width_, render_height_,
			GL_DEPTH_BUFFER_BIT,
			GL_NEAREST
		);

		if (scissor_enabled) {
			glEnable(GL_SCISSOR_TEST);
		}

		// Restore previous framebuffer bindings
		glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);

		generate_shader_->use();

		// Mip 0 source is the temporary, pure depth buffer.
		// All subsequent mips source from the previous Hi-Z mip.
		int src_w = render_width_;
		int src_h = render_height_;

		for (int mip = 0; mip < mip_count_; ++mip) {
			int dst_w = std::max(1, hiz_width_ >> mip);
			int dst_h = std::max(1, hiz_height_ >> mip);

			// Bind source texture
			glActiveTexture(GL_TEXTURE0);
			if (mip == 0) {
				// Mip 0: 2x MAX downsample from our temp, pure depth_texture_ (GL_DEPTH_COMPONENT32F)
				glBindTexture(GL_TEXTURE_2D, temp_depth_texture_);
				generate_shader_->setInt("u_srcLevel", 0);
			} else {
				// Mip N: 2x MAX downsample from previous Hi-Z mip
				glBindTexture(GL_TEXTURE_2D, hiz_texture_);
				generate_shader_->setInt("u_srcLevel", mip - 1);
			}
			generate_shader_->setInt("u_srcDepth", 0);

			// Bind destination mip as image
			glBindImageTexture(0, hiz_texture_, mip, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

			// Dispatch
			glDispatchCompute((dst_w + 7) / 8, (dst_h + 7) / 8, 1);

			// Barrier before next mip reads the result
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

			// Next iteration's source size is this mip's size
			src_w = dst_w;
			src_h = dst_h;
		}

		// Reset texture parameters if we modified them
		glBindTexture(GL_TEXTURE_2D, hiz_texture_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip_count_ - 1);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Ensure all Hi-Z mip data is visible to subsequent texture fetches.
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
	}

} // namespace Boidsish
