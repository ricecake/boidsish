#pragma once

#include <memory>

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {

	struct NoiseTextures {
		GLuint noise;
		GLuint curl;
		GLuint cloud_base;
		GLuint cloud_detail;
		GLuint weather_map;
	};

	class NoiseManager {
	public:
		NoiseManager();
		~NoiseManager();

		void Initialize();
		void Generate();

		GLuint GetNoiseTexture() const { return noise_texture_; }

		GLuint GetCurlTexture() const { return curl_noise_texture_; }

		GLuint GetCloudBaseTexture() const { return cloud_base_texture_; }

		GLuint GetCloudDetailTexture() const { return cloud_detail_texture_; }

		GLuint GetWeatherMap() const { return weather_map_; }

		NoiseTextures GetTextures() const {
			return {noise_texture_, curl_noise_texture_, cloud_base_texture_, cloud_detail_texture_, weather_map_};
		}

		void Bind(GLuint unit) const;
		void BindDefault(class ShaderBase& shader) const;

	private:
		std::unique_ptr<ComputeShader> compute_shader_;
		GLuint                         noise_texture_ = 0;
		GLuint                         curl_noise_texture_ = 0;
		GLuint                         cloud_base_texture_ = 0;
		GLuint                         cloud_detail_texture_ = 0;
		GLuint                         weather_map_ = 0;

		int size_ = 64;           // 64x64x64
		int cloud_base_size_ = 128; // 128x128x128
		int cloud_detail_size_ = 32; // 32x32x32
		int weather_map_size_ = 1024; // 1024x1024
	};

} // namespace Boidsish
