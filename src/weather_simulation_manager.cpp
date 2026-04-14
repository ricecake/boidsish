#include "weather_simulation_manager.h"
#include <GL/glew.h>
#include "constants.h"
#include "profiler.h"
#include "logger.h"
#include "terrain_render_manager.h"

namespace Boidsish {

	WeatherSimulationManager::WeatherSimulationManager(int width, int height, float cellSize)
		: width_(width), height_(height), cell_size_(cellSize) {

		Initialize();
	}

	WeatherSimulationManager::~WeatherSimulationManager() {
		glDeleteBuffers(2, ssbo_);
		if (ubo_ != 0) glDeleteBuffers(1, &ubo_);
	}

	void WeatherSimulationManager::Initialize() {
		init_shader_ = std::make_unique<ComputeShader>("shaders/weather_lbm_init.comp");
		step_shader_ = std::make_unique<ComputeShader>("shaders/weather_lbm_step.comp");

		if (!init_shader_->isValid() || !step_shader_->isValid()) {
			logger::ERROR("Failed to load LBM weather simulation shaders");
		}

		glGenBuffers(2, ssbo_);
		size_t bufferSize = width_ * height_ * sizeof(LbmCell);

		for (int i = 0; i < 2; ++i) {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_[i]);
			glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
		}

		glGenBuffers(1, &ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(WeatherUniforms), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// Initial state
		if (init_shader_->isValid()) {
			init_shader_->use();
			init_shader_->setUint("u_width", width_);
			init_shader_->setUint("u_height", height_);
			init_shader_->setFloat("u_initialTemperature", 15.0f); // Default 15C

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::WeatherGridA(), ssbo_[0]);
			glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}

	void WeatherSimulationManager::Update(
		float                            deltaTime,
		float                            globalTemperature,
		const std::vector<AerosolSource>& sources,
		const TerrainRenderManager*      terrainRenderManager
	) {
		if (!step_shader_ || !step_shader_->isValid())
			return;

		PROJECT_PROFILE_SCOPE("WeatherSimulationManager::Update");

		step_shader_->use();
		step_shader_->setFloat("u_deltaTime", deltaTime);

		// Pass aerosol sources (max 8 for now for simplicity, can use SSBO later if needed)
		int numSources = std::min((int)sources.size(), 8);
		step_shader_->setInt("u_numAerosolSources", numSources);
		for (int i = 0; i < numSources; ++i) {
			std::string prefix = "u_aerosolSources[" + std::to_string(i) + "].";
			step_shader_->setVec2(prefix + "position", sources[i].position);
			step_shader_->setFloat(prefix + "strength", sources[i].strength);
			step_shader_->setFloat(prefix + "radius", sources[i].radius);
		}

		// Terrain data for boundaries and drag
		if (terrainRenderManager) {
			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_2D, terrainRenderManager->GetChunkGridTexture());
			step_shader_->setInt("u_chunkGrid", 5);

			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_2D_ARRAY, terrainRenderManager->GetHeightmapTexture());
			step_shader_->setInt("u_heightmapArray", 6);

			terrainRenderManager->BindTerrainData(*step_shader_);
		}

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::WeatherGridA(), ssbo_[read_idx_]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::WeatherGridB(), ssbo_[write_idx_]);

		// Update and bind WeatherUniforms UBO
		WeatherUniforms uboData{};
		uboData.originAndSize = glm::vec4(0.0f, 0.0f, (float)width_, (float)height_);
		uboData.params = glm::vec4(cell_size_, globalTemperature, 0.0f, 0.0f);

		glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(WeatherUniforms), &uboData);
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::WeatherUniforms(), ubo_);

		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		std::swap(read_idx_, write_idx_);
	}

} // namespace Boidsish
