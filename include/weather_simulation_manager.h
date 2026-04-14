#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "shader.h"

namespace Boidsish {

	class TerrainRenderManager;

	struct AerosolSource {
		glm::vec2 position;
		float     strength;
		float     radius;
	};

	class WeatherSimulationManager {
	public:
		WeatherSimulationManager(int width, int height, float cellSize);
		~WeatherSimulationManager();

		void Update(
			float                            deltaTime,
			float                            globalTemperature,
			const std::vector<AerosolSource>& sources,
			const TerrainRenderManager*      terrainRenderManager
		);

		uint32_t GetCurrentReadBuffer() const { return ssbo_[read_idx_]; }
		int GetWidth() const { return width_; }
		int GetHeight() const { return height_; }
		float GetCellSize() const { return cell_size_; }

	private:
		struct WeatherUniforms {
			glm::vec4 originAndSize; // xy = origin, zw = size
			glm::vec4 params;        // x = cellSize, y = globalTemp, zw = unused
		};

		void Initialize();

		int   width_;
		int   height_;
		float cell_size_;

		uint32_t ssbo_[2];
		uint32_t ubo_ = 0;
		int      read_idx_ = 0;
		int      write_idx_ = 1;

		std::unique_ptr<ComputeShader> init_shader_;
		std::unique_ptr<ComputeShader> step_shader_;

		struct LbmCell {
			float f[9];
			float temperature;
			float aerosol;
			float vVel;
		};
	};

} // namespace Boidsish
