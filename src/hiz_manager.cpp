#include "hiz_manager.h"

#include "shader.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace Boidsish {

HiZManager::HiZManager() = default;

HiZManager::~HiZManager() {
	DestroyTexture();
}

void HiZManager::Initialize(int width, int height) {
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
}

void HiZManager::DestroyTexture() {
	if (hiz_texture_) {
		glDeleteTextures(1, &hiz_texture_);
		hiz_texture_ = 0;
	}
}

void HiZManager::GeneratePyramid(GLuint depthTexture) {
	if (!initialized_ || !generate_shader_->isValid())
		return;

	generate_shader_->use();

	// Mip 0 source is the full-resolution depth buffer.
	// All subsequent mips source from the previous Hi-Z mip.
	int src_w = render_width_;
	int src_h = render_height_;

	for (int mip = 0; mip < mip_count_; ++mip) {
		int dst_w = std::max(1, hiz_width_ >> mip);
		int dst_h = std::max(1, hiz_height_ >> mip);

		// Set source size uniform
		glUniform2i(glGetUniformLocation(generate_shader_->ID, "u_srcSize"), src_w, src_h);

		// Bind source texture
		glActiveTexture(GL_TEXTURE0);
		if (mip == 0) {
			// Mip 0: 2x MAX downsample from full-res depth buffer → half-res Hi-Z base
			glBindTexture(GL_TEXTURE_2D, depthTexture);
		} else {
			// Mip N: 2x MAX downsample from previous Hi-Z mip
			glBindTexture(GL_TEXTURE_2D, hiz_texture_);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, mip - 1);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip - 1);
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
