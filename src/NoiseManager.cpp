#include "NoiseManager.h"

#include <GL/glew.h>
#include <random>

#include "constants.h"
#include "gpu_resource_registry.h"
#include "logger.h"
#include "profiler.h"
#include "service_locator.h"

namespace Boidsish {

	NoiseManager::NoiseManager(ServiceLocator& /*loc*/) {}

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
		if (extra_noise_texture_ != 0) {
			glDeleteTextures(1, &extra_noise_texture_);
		}
		if (phasor_noise_texture_ != 0) {
			glDeleteTextures(1, &phasor_noise_texture_);
		}
		if (nca_3d_texture_ != 0) {
			glDeleteTextures(1, &nca_3d_texture_);
		}
		if (nca_3d_temp_texture_ != 0) {
			glDeleteTextures(1, &nca_3d_temp_texture_);
		}
		if (nca_2d_texture_ != 0) {
			glDeleteTextures(1, &nca_2d_texture_);
		}
		if (nca_2d_temp_texture_ != 0) {
			glDeleteTextures(1, &nca_2d_temp_texture_);
		}
	}

	void NoiseManager::Initialize() {
		PROJECT_PROFILE_SCOPE("NoiseManager::Initialize");
		compute_shader_ = std::make_unique<ComputeShader>("shaders/noise_gen.comp");
		if (!compute_shader_->isValid()) {
			logger::ERROR("Failed to compile noise generation compute shader");
		}

		blue_noise_shader_ = std::make_unique<ComputeShader>("shaders/blue_noise_gen.comp");
		if (!blue_noise_shader_->isValid()) {
			logger::ERROR("Failed to compile blue noise generation compute shader");
		}

		phasor_gen_shader_ = std::make_unique<ComputeShader>("shaders/phasor_gen.comp");
		if (!phasor_gen_shader_->isValid()) {
			logger::ERROR("Failed to compile phasor noise generation compute shader");
		}

		mlp_nca_3d_shader_ = std::make_unique<ComputeShader>("shaders/mlp_nca_3d.comp");
		if (!mlp_nca_3d_shader_->isValid()) {
			logger::ERROR("Failed to compile 3D NCA compute shader");
		}

		mlp_nca_2d_shader_ = std::make_unique<ComputeShader>("shaders/mlp_nca.comp");
		if (!mlp_nca_2d_shader_->isValid()) {
			logger::ERROR("Failed to compile 2D NCA compute shader");
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

		glGenTextures(1, &extra_noise_texture_);
		glBindTexture(GL_TEXTURE_3D, extra_noise_texture_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, size_, size_, size_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

		glGenTextures(1, &phasor_noise_texture_);
		glBindTexture(GL_TEXTURE_2D, phasor_noise_texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, blue_noise_size_, blue_noise_size_, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glGenTextures(1, &nca_3d_texture_);
		glBindTexture(GL_TEXTURE_3D, nca_3d_texture_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, size_, size_, size_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

		glGenTextures(1, &nca_3d_temp_texture_);
		glBindTexture(GL_TEXTURE_3D, nca_3d_temp_texture_);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, size_, size_, size_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

		glGenTextures(1, &nca_2d_texture_);
		glBindTexture(GL_TEXTURE_2D, nca_2d_texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, size_, size_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glGenTextures(1, &nca_2d_temp_texture_);
		glBindTexture(GL_TEXTURE_2D, nca_2d_temp_texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, size_, size_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glBindTexture(GL_TEXTURE_3D, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Initialize NCA networks
		nca_net_.Initialize({16, 16, 16, 4}, {5, 5, 0});
		nca_net_.RandomizeWeights();

		nca_2d_net_.Initialize({12, 16, 16, 4}, {5, 5, 0});
		nca_2d_net_.RandomizeWeights();

		// Seed the 3D NCA texture
		Seed3DNCA();

		// Seed the 2D NCA texture
		Seed2DPoint();

		Generate();

		auto& reg = GpuResourceRegistry::Instance();
		reg.PublishTexture(Constants::TextureUnit::NoiseSimplex(), noise_texture_, GL_TEXTURE_3D);
		reg.PublishTexture(Constants::TextureUnit::NoiseCurl(), curl_noise_texture_, GL_TEXTURE_3D);
		reg.PublishTexture(Constants::TextureUnit::NoiseBlue(), blue_noise_texture_);
		reg.PublishTexture(Constants::TextureUnit::NoiseExtra(), extra_noise_texture_, GL_TEXTURE_3D);
		reg.PublishTexture(Constants::TextureUnit::NoisePhasor(), phasor_noise_texture_);
		reg.PublishTexture(Constants::TextureUnit::NoiseNca3D(), nca_3d_texture_, GL_TEXTURE_3D);
		reg.PublishTexture(Constants::TextureUnit::NoiseNca2D(), nca_2d_texture_, GL_TEXTURE_2D);
	}

	void NoiseManager::Generate() {
		PROJECT_PROFILE_SCOPE("NoiseManager::Generate");
		if (compute_shader_ && compute_shader_->isValid()) {
			compute_shader_->use();
			glBindImageTexture(0, noise_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			glBindImageTexture(1, curl_noise_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			glBindImageTexture(2, extra_noise_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

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

		if (phasor_gen_shader_ && phasor_gen_shader_->isValid()) {
			phasor_gen_shader_->use();
			glBindImageTexture(0, phasor_noise_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			// local_size is 16x16
			glDispatchCompute(blue_noise_size_ / 16, blue_noise_size_ / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		if (mlp_nca_3d_shader_ && mlp_nca_3d_shader_->isValid()) {
			mlp_nca_3d_shader_->use();
			nca_net_.Bind(57); // Bind MLP params

			GLuint read_tex = nca_3d_texture_;
			GLuint write_tex = nca_3d_temp_texture_;

			// Run 32 steps of 3D NCA
			for (int step = 0; step < 32; ++step) {
				glBindImageTexture(0, read_tex, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
				glBindImageTexture(1, write_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

				mlp_nca_3d_shader_->setFloat("u_time", static_cast<float>(step) * 0.1f);
				mlp_nca_3d_shader_->setFloat("u_step_size", 1.0f);
				mlp_nca_3d_shader_->setFloat("u_update_probability", 0.5f);

				// local_size is 4x4x4, so dispatch size/4
				glDispatchCompute(size_ / 4, size_ / 4, size_ / 4);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				std::swap(read_tex, write_tex);
			}

			// If the latest state ended up in nca_3d_temp_texture_, swap the actual handles.
			if (read_tex != nca_3d_texture_) {
				std::swap(nca_3d_texture_, nca_3d_temp_texture_);
			}
		}

		if (mlp_nca_2d_shader_ && mlp_nca_2d_shader_->isValid()) {
			mlp_nca_2d_shader_->use();
			nca_2d_net_.Bind(57); // Bind MLP params

			GLuint read_tex = nca_2d_texture_;
			GLuint write_tex = nca_2d_temp_texture_;

			// Run 32 steps of 2D NCA
			for (int step = 0; step < 32; ++step) {
				glBindImageTexture(0, read_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
				glBindImageTexture(1, write_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

				mlp_nca_2d_shader_->setFloat("u_time", static_cast<float>(step) * 0.1f);
				mlp_nca_2d_shader_->setFloat("u_step_size", 1.0f);
				mlp_nca_2d_shader_->setFloat("u_update_probability", 0.5f);

				// local_size is 16x16, so dispatch size/16
				glDispatchCompute(size_ / 16, size_ / 16, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				std::swap(read_tex, write_tex);
			}

			// If the latest state ended up in nca_2d_temp_texture_, swap the actual handles.
			if (read_tex != nca_2d_texture_) {
				std::swap(nca_2d_texture_, nca_2d_temp_texture_);
			}
		}

		glBindImageTexture(0, 0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glBindImageTexture(1, 0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		glBindImageTexture(2, 0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	}

	void NoiseManager::Seed3DNCA() {
		std::vector<float> pixels(size_ * size_ * size_ * 4, 0.0f);
		int cx = size_ / 2;
		int cy = size_ / 2;
		int cz = size_ / 2;
		int idx = ((cz * size_ + cy) * size_ + cx) * 4;
		pixels[idx + 0] = 1.0f; // R
		pixels[idx + 1] = 0.0f; // G
		pixels[idx + 2] = 0.0f; // B
		pixels[idx + 3] = 1.0f; // A

		glBindTexture(GL_TEXTURE_3D, nca_3d_texture_);
		glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, size_, size_, size_, GL_RGBA, GL_FLOAT, pixels.data());
		glBindTexture(GL_TEXTURE_3D, 0);
	}

	void NoiseManager::Update(float time, float dt) {
		// Only step when the simulation is not paused
		if (dt <= 0.0f) return;

		if (mlp_nca_3d_shader_ && mlp_nca_3d_shader_->isValid()) {
			mlp_nca_3d_shader_->use();
			nca_net_.Bind(57); // Bind MLP params

			GLuint read_tex = nca_3d_texture_;
			GLuint write_tex = nca_3d_temp_texture_;

			glBindImageTexture(0, read_tex, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
			glBindImageTexture(1, write_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

			mlp_nca_3d_shader_->setFloat("u_time", time);
			mlp_nca_3d_shader_->setFloat("u_step_size", 1.0f);
			mlp_nca_3d_shader_->setFloat("u_update_probability", 0.5f);

			// local_size is 4x4x4, so dispatch size/4
			glDispatchCompute(size_ / 4, size_ / 4, size_ / 4);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Swap actual texture IDs so nca_3d_texture_ holds the latest state
			std::swap(nca_3d_texture_, nca_3d_temp_texture_);

			// Re-publish the updated texture handle to GpuResourceRegistry
			auto& reg = GpuResourceRegistry::Instance();
			reg.PublishTexture(Constants::TextureUnit::NoiseNca3D(), nca_3d_texture_, GL_TEXTURE_3D);
		}

		if (mlp_nca_2d_shader_ && mlp_nca_2d_shader_->isValid()) {
			mlp_nca_2d_shader_->use();
			nca_2d_net_.Bind(57); // Bind MLP params

			GLuint read_tex = nca_2d_texture_;
			GLuint write_tex = nca_2d_temp_texture_;

			glBindImageTexture(0, read_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
			glBindImageTexture(1, write_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

			mlp_nca_2d_shader_->setFloat("u_time", time);
			mlp_nca_2d_shader_->setFloat("u_step_size", 1.0f);
			mlp_nca_2d_shader_->setFloat("u_update_probability", 0.5f);

			// local_size is 16x16, so dispatch size/16
			glDispatchCompute(size_ / 16, size_ / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Swap actual texture IDs so nca_2d_texture_ holds the latest state
			std::swap(nca_2d_texture_, nca_2d_temp_texture_);

			// Re-publish the updated texture handle to GpuResourceRegistry
			auto& reg = GpuResourceRegistry::Instance();
			reg.PublishTexture(Constants::TextureUnit::NoiseNca2D(), nca_2d_texture_, GL_TEXTURE_2D);
		}
	}

	void NoiseManager::Bind(GLuint unit) const {
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_3D, noise_texture_);
		glActiveTexture(GL_TEXTURE0 + unit + 1);
		glBindTexture(GL_TEXTURE_3D, curl_noise_texture_);
		glActiveTexture(GL_TEXTURE0 + unit + 2);
		glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
		glActiveTexture(GL_TEXTURE0 + unit + 3);
		glBindTexture(GL_TEXTURE_3D, extra_noise_texture_);

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoisePhasor());
		glBindTexture(GL_TEXTURE_2D, phasor_noise_texture_);

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseNca3D());
		glBindTexture(GL_TEXTURE_3D, nca_3d_texture_);

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseNca2D());
		glBindTexture(GL_TEXTURE_2D, nca_2d_texture_);
	}

	void NoiseManager::BindDefault(ShaderBase& shader) const {
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseSimplex());
		glBindTexture(GL_TEXTURE_3D, noise_texture_);
		shader.trySetInt("u_noiseTexture", Constants::TextureUnit::NoiseSimplex());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseCurl());
		glBindTexture(GL_TEXTURE_3D, curl_noise_texture_);
		shader.trySetInt("u_curlTexture", Constants::TextureUnit::NoiseCurl());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseBlue());
		glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
		shader.trySetInt("u_blueNoiseTexture", Constants::TextureUnit::NoiseBlue());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseExtra());
		glBindTexture(GL_TEXTURE_3D, extra_noise_texture_);
		shader.trySetInt("u_extraNoiseTexture", Constants::TextureUnit::NoiseExtra());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoisePhasor());
		glBindTexture(GL_TEXTURE_2D, phasor_noise_texture_);
		shader.trySetInt("u_phasorTexture", Constants::TextureUnit::NoisePhasor());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseNca3D());
		glBindTexture(GL_TEXTURE_3D, nca_3d_texture_);
		shader.trySetInt("u_nca3DTexture", Constants::TextureUnit::NoiseNca3D());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseNca2D());
		glBindTexture(GL_TEXTURE_2D, nca_2d_texture_);
		shader.trySetInt("u_nca2DTexture", Constants::TextureUnit::NoiseNca2D());
	}

	void NoiseManager::Randomize2DWeights() {
		nca_2d_net_.RandomizeWeights();
	}

	void NoiseManager::Seed2DPoint() {
		std::vector<float> pixels(size_ * size_ * 4, 0.0f);
		int cx = size_ / 2;
		int cy = size_ / 2;
		int idx = (cy * size_ + cx) * 4;
		pixels[idx + 0] = 1.0f; // R
		pixels[idx + 1] = 0.0f; // G
		pixels[idx + 2] = 0.0f; // B
		pixels[idx + 3] = 1.0f; // A

		glBindTexture(GL_TEXTURE_2D, nca_2d_texture_);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size_, size_, GL_RGBA, GL_FLOAT, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void NoiseManager::Seed2DNoise() {
		std::vector<float> pixels(size_ * size_ * 4);
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(0.0f, 1.0f);
		for (auto& val : pixels) {
			val = dis(gen);
		}

		glBindTexture(GL_TEXTURE_2D, nca_2d_texture_);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size_, size_, GL_RGBA, GL_FLOAT, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void NoiseManager::Randomize3DWeights() {
		nca_net_.RandomizeWeights();
	}

	void NoiseManager::Seed3DPoint() {
		Seed3DNCA();
	}

	void NoiseManager::Seed3DNoise() {
		std::vector<float> pixels(size_ * size_ * size_ * 4);
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(0.0f, 1.0f);
		for (auto& val : pixels) {
			val = dis(gen);
		}

		glBindTexture(GL_TEXTURE_3D, nca_3d_texture_);
		glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, size_, size_, size_, GL_RGBA, GL_FLOAT, pixels.data());
		glBindTexture(GL_TEXTURE_3D, 0);
	}

} // namespace Boidsish
