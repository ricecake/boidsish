#pragma once

#include <memory>

#include "IManager.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {

	struct NoiseTextures {
		GLuint noise;
		GLuint curl;
		GLuint blue_noise;
		GLuint extra_noise;
	};

	class NoiseManager: public IManager {
	public:
		NoiseManager();
		~NoiseManager() override;

		void Initialize() override;
		void Generate();

		GLuint GetNoiseTexture() const { return noise_texture_; }

		GLuint GetCurlTexture() const { return curl_noise_texture_; }

		GLuint GetBlueNoiseTexture() const { return blue_noise_texture_; }

		GLuint GetExtraNoiseTexture() const { return extra_noise_texture_; }

		NoiseTextures GetTextures() const {
			return {noise_texture_, curl_noise_texture_, blue_noise_texture_, extra_noise_texture_};
		}

		void Bind(GLuint unit) const;
		void BindDefault(class ShaderBase& shader) const;

	private:
		std::unique_ptr<ComputeShader> compute_shader_;
		std::unique_ptr<ComputeShader> blue_noise_shader_;
		GLuint                         noise_texture_ = 0;
		GLuint                         curl_noise_texture_ = 0;
		GLuint                         blue_noise_texture_ = 0;
		GLuint                         extra_noise_texture_ = 0;
		int                            size_ = 64; // 64x64x64
		int                            blue_noise_size_ = 256;
	};

} // namespace Boidsish
