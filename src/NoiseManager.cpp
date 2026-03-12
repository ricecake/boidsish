#include "NoiseManager.h"

#include "logger.h"
#include <GL/glew.h>

namespace Boidsish {

	NoiseManager::NoiseManager() {}

	NoiseManager::~NoiseManager() {
		if (noise_texture_ != 0) {
			glDeleteTextures(1, &noise_texture_);
		}
		if (curl_noise_texture_ != 0) {
			glDeleteTextures(1, &curl_noise_texture_);
		}
		if (blue_noise_texture_ != 0) {
			glDeleteTextures(1, &blue_noise_texture_);
		}
	}

	void NoiseManager::Initialize() {
		compute_shader_ = std::make_unique<ComputeShader>("shaders/noise_gen.comp");
		if (!compute_shader_->isValid()) {
			logger::ERROR("Failed to compile noise generation compute shader");
		}

		blue_noise_shader_ = std::make_unique<ComputeShader>("shaders/blue_noise_gen.comp");
		if (!blue_noise_shader_->isValid()) {
			logger::ERROR("Failed to compile blue noise generation compute shader");
		}

		glGenTextures(1, &noise_texture_);
		glBindTexture(GL_TEXTURE_3D, noise_texture_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, size_, size_, size_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

		glGenTextures(1, &curl_noise_texture_);
		glBindTexture(GL_TEXTURE_3D, curl_noise_texture_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, size_, size_, size_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

		glGenTextures(1, &blue_noise_texture_);
		glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, blue_noise_size_, blue_noise_size_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glBindTexture(GL_TEXTURE_3D, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		Generate();
	}

	void NoiseManager::Generate() {
		if (compute_shader_ && compute_shader_->isValid()) {
			compute_shader_->use();
			glBindImageTexture(0, noise_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			glBindImageTexture(1, curl_noise_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

			// local_size in shader is 4x4x4, so dispatch size/4
			glDispatchCompute(size_ / 4, size_ / 4, size_ / 4);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		if (blue_noise_shader_ && blue_noise_shader_->isValid()) {
			blue_noise_shader_->use();
			glBindImageTexture(0, blue_noise_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

			// local_size is 16x16
			glDispatchCompute(blue_noise_size_ / 16, blue_noise_size_ / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		glBindImageTexture(0, 0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glBindImageTexture(1, 0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	}

	void NoiseManager::Bind(GLuint unit) const {
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_3D, noise_texture_);
		glActiveTexture(GL_TEXTURE0 + unit + 1);
		glBindTexture(GL_TEXTURE_3D, curl_noise_texture_);
		glActiveTexture(GL_TEXTURE0 + unit + 2);
		glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
	}

	void NoiseManager::BindDefault(ShaderBase& shader) const {
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_3D, noise_texture_);
		shader.trySetInt("u_noiseTexture", 5);
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_3D, curl_noise_texture_);
		shader.trySetInt("u_curlTexture", 6);
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
		shader.trySetInt("u_blueNoiseTexture", 7);
	}

} // namespace Boidsish
