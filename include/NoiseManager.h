#pragma once

#include <memory>

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {

	struct NoiseTextures {
		GLuint noise;
		GLuint curl;
	};

	class NoiseManager {
	public:
		NoiseManager();
		~NoiseManager();

		void Initialize();
		void Generate();

		GLuint GetNoiseTexture() const { return noise_texture_; }

		GLuint GetCurlTexture() const { return curl_noise_texture_; }

		GLuint64 GetNoiseHandle() const { return noise_handle_; }

		GLuint64 GetCurlHandle() const { return curl_noise_handle_; }

		NoiseTextures GetTextures() const { return {noise_texture_, curl_noise_texture_}; }

		void Bind(GLuint unit) const;
		void BindDefault(class ShaderBase& shader) const;

	private:
		std::unique_ptr<ComputeShader> compute_shader_;
		GLuint                         noise_texture_ = 0;
		GLuint                         curl_noise_texture_ = 0;
		GLuint64                       noise_handle_ = 0;
		GLuint64                       curl_noise_handle_ = 0;
		int                            size_ = 64; // 64x64x64
	};

} // namespace Boidsish
