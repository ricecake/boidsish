#pragma once

#include <memory>

#include "IManager.h"
#include "shader.h"
#include "mlp_network.h"
#include <GL/glew.h>

namespace Boidsish {

	class ServiceLocator;

	struct NoiseTextures {
		GLuint noise;
		GLuint curl;
		GLuint blue_noise;
		GLuint extra_noise;
		GLuint phasor;
		GLuint nca_3d;
	};

	class NoiseManager: public IManager {
	public:
		NoiseManager(ServiceLocator& loc);
		~NoiseManager() override;

		void Initialize() override;
		void Generate();

		GLuint GetNoiseTexture() const { return noise_texture_; }

		GLuint GetCurlTexture() const { return curl_noise_texture_; }

		GLuint GetBlueNoiseTexture() const { return blue_noise_texture_; }

		GLuint GetExtraNoiseTexture() const { return extra_noise_texture_; }

		GLuint GetPhasorNoiseTexture() const { return phasor_noise_texture_; }

		GLuint GetNca3DTexture() const { return nca_3d_texture_; }

		NoiseTextures GetTextures() const {
			return {
				noise_texture_,
				curl_noise_texture_,
				blue_noise_texture_,
				extra_noise_texture_,
				phasor_noise_texture_,
				nca_3d_texture_};
		}

		void Bind(GLuint unit) const;
		void BindDefault(class ShaderBase& shader) const;
		void Seed3DNCA();
		void Update(float time, float dt);

	private:
		std::unique_ptr<ComputeShader> compute_shader_;
		std::unique_ptr<ComputeShader> blue_noise_shader_;
		std::unique_ptr<ComputeShader> phasor_gen_shader_;
		std::unique_ptr<ComputeShader> mlp_nca_3d_shader_;
		GLuint                         noise_texture_ = 0;
		GLuint                         curl_noise_texture_ = 0;
		GLuint                         blue_noise_texture_ = 0;
		GLuint                         extra_noise_texture_ = 0;
		GLuint                         phasor_noise_texture_ = 0;
		GLuint                         nca_3d_texture_ = 0;
		GLuint                         nca_3d_temp_texture_ = 0;
		MLPNetwork                     nca_net_;
		int                            size_ = 64; // 64x64x64
		int                            blue_noise_size_ = 1024;
	};

} // namespace Boidsish
