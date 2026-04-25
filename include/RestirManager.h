#ifndef RESTIR_MANAGER_H
#define RESTIR_MANAGER_H

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "shader.h"
#include "IManager.h"

namespace Boidsish {

	class ServiceLocator;
	class LightManager;
	class FireEffectManager;

	struct ReservoirCPU {
		uint32_t light_index;
		float    w_sum;
		float    m;
		float    W;
	};

	class RestirManager : public IManager {
	public:
		RestirManager(ServiceLocator& loc);
		~RestirManager();

		void Initialize() override;
		void Update(int width, int height, float time);
		void Dispatch(
			GLuint depthTex,
			GLuint normalTex,
			GLuint velocityTex,
			GLuint colorTex,
			GLuint albedoTex,
			LightManager& lightManager,
			FireEffectManager* fireManager
		);

		void Resize(int width, int height);

		GLuint GetCurrentDIRiservoirBuffer() const { return reservoir_buffers_[current_buffer_index_]; }
		GLuint GetCurrentGIRiservoirBuffer() const { return gi_reservoir_buffers_[current_buffer_index_]; }

	private:
		ServiceLocator& loc_;
		std::unique_ptr<ComputeShader> di_sampling_shader_;
		std::unique_ptr<ComputeShader> di_temporal_shader_;
		std::unique_ptr<ComputeShader> di_spatial_shader_;
		std::unique_ptr<ComputeShader> gi_trace_shader_;
		std::unique_ptr<ComputeShader> gi_reuse_shader_;

		GLuint reservoir_buffers_[2] = {0, 0};
		GLuint gi_reservoir_buffers_[2] = {0, 0};
		int    current_buffer_index_ = 0;
		int    width_ = 0, height_ = 0;

		void CreateBuffers(int width, int height);
	};

} // namespace Boidsish

#endif // RESTIR_MANAGER_H
