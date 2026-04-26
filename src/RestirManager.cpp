#include "RestirManager.h"
#include "service_locator.h"
#include "light_manager.h"
#include "fire_effect_manager.h"
#include "constants.h"
#include "logger.h"
#include "profiler.h"
#include <GL/glew.h>
#include <random>
#include <numeric>
#include <algorithm>

namespace Boidsish {

	RestirManager::RestirManager(ServiceLocator& loc) : loc_(loc) {}

	RestirManager::~RestirManager() {
		if (reservoir_buffers_[0]) glDeleteBuffers(2, reservoir_buffers_);
		if (gi_reservoir_buffers_[0]) glDeleteBuffers(2, gi_reservoir_buffers_);
		if (permutation_tex_) glDeleteTextures(1, &permutation_tex_);
	}

	void RestirManager::Initialize() {
		di_sampling_shader_ = std::make_unique<ComputeShader>("shaders/restir/di_sampling.comp");
		di_temporal_shader_ = std::make_unique<ComputeShader>("shaders/restir/di_temporal.comp");
		di_spatial_shader_ = std::make_unique<ComputeShader>("shaders/restir/di_spatial.comp");
		gi_trace_shader_ = std::make_unique<ComputeShader>("shaders/restir/gi_trace.comp");
		gi_reuse_shader_ = std::make_unique<ComputeShader>("shaders/restir/gi_reuse.comp");
		gi_spatial_shader_ = std::make_unique<ComputeShader>("shaders/restir/gi_spatial.comp");

		auto setup_comp = [&](ComputeShader* s) {
			if (!s || !s->isValid()) return;
			s->use();
			GLuint temporal_idx = glGetUniformBlockIndex(s->ID, "TemporalData");
			if (temporal_idx != GL_INVALID_INDEX) glUniformBlockBinding(s->ID, temporal_idx, Constants::UboBinding::TemporalData());
		};

		setup_comp(di_sampling_shader_.get());
		setup_comp(di_temporal_shader_.get());
		setup_comp(di_spatial_shader_.get());
		setup_comp(gi_trace_shader_.get());
		setup_comp(gi_reuse_shader_.get());
		setup_comp(gi_spatial_shader_.get());
	}

	void RestirManager::CreateBuffers(int width, int height) {
		if (reservoir_buffers_[0]) glDeleteBuffers(2, reservoir_buffers_);
		if (gi_reservoir_buffers_[0]) glDeleteBuffers(2, gi_reservoir_buffers_);
		if (permutation_tex_) glDeleteTextures(1, &permutation_tex_);

		size_t size = width * height * sizeof(ReservoirCPU);
		glGenBuffers(2, reservoir_buffers_);
		glGenBuffers(2, gi_reservoir_buffers_);

		for (int i = 0; i < 2; i++) {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, reservoir_buffers_[i]);
			glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, gi_reservoir_buffers_[i]);
			glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Create permutation texture array (128x128 x 8 slices)
		int p_size = 128;
		int p_slices = 8;
		glGenTextures(1, &permutation_tex_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, permutation_tex_);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RG8_SNORM, p_size, p_size, p_slices, 0, GL_RG, GL_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

		for (int i = 0; i < p_slices; i++) {
			auto sliceData = generateReciprocalPermutation(p_size, 8, 1337 + i);
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, p_size, p_size, 1, GL_RG, GL_BYTE, sliceData.data());
		}
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	std::vector<int8_t> RestirManager::generateReciprocalPermutation(int size, int maxRadius, uint32_t seed) {
		struct Int2 { int x, y; };
		int totalCells = size * size;
		// Use 127 as a sentinel value for "unpaired".
		std::vector<Int2> grid(totalCells, {127, 127});

		std::vector<int> indices(totalCells);
		std::iota(indices.begin(), indices.end(), 0);

		std::mt19937 rng(seed);
		std::shuffle(indices.begin(), indices.end(), rng);

		std::vector<Int2> possibleOffsets;
		for (int dy = -maxRadius; dy <= maxRadius; ++dy) {
			for (int dx = -maxRadius; dx <= maxRadius; ++dx) {
				if (dx == 0 && dy == 0) continue;
				if (dx * dx + dy * dy <= maxRadius * maxRadius) {
					possibleOffsets.push_back({dx, dy});
				}
			}
		}

		for (int idx : indices) {
			if (grid[idx].x != 127) continue;

			int x = idx % size;
			int y = idx / size;

			std::shuffle(possibleOffsets.begin(), possibleOffsets.end(), rng);

			bool paired = false;
			for (const auto& offset : possibleOffsets) {
				int nx = ((x + offset.x) % size + size) % size;
				int ny = ((y + offset.y) % size + size) % size;
				int nIdx = ny * size + nx;

				if (grid[nIdx].x == 127) {
					grid[idx] = offset;
					grid[nIdx] = {-offset.x, -offset.y};
					paired = true;
					break;
				}
			}

			if (!paired) {
				for (int fallbackIdx = 0; fallbackIdx < totalCells; ++fallbackIdx) {
					if (fallbackIdx != idx && grid[fallbackIdx].x == 127) {
						int fx = fallbackIdx % size;
						int fy = fallbackIdx / size;

						int dx = fx - x;
						int dy = fy - y;
						if (dx > size / 2) dx -= size;
						if (dx < -size / 2) dx += size;
						if (dy > size / 2) dy -= size;
						if (dy < -size / 2) dy += size;

						grid[idx] = {dx, dy};
						grid[fallbackIdx] = {-dx, -dy};
						break;
					}
				}
			}
		}

		std::vector<int8_t> textureData;
		textureData.reserve(totalCells * 2);
		for (const auto& cell : grid) {
			textureData.push_back(static_cast<int8_t>(cell.x));
			textureData.push_back(static_cast<int8_t>(cell.y));
		}

		return textureData;
	}

	void RestirManager::Resize(int width, int height) {
		width_ = width;
		height_ = height;
		CreateBuffers(width, height);
	}

	void RestirManager::Update(int width, int height, float /* time */) {
		if (width != width_ || height != height_) {
			Resize(width, height);
		}
	}

	void RestirManager::Dispatch(
		GLuint depthTex,
		GLuint normalTex,
		GLuint velocityTex,
		GLuint colorTex,
		GLuint albedoTex,
		GLuint blueNoiseTex,
		LightManager& lightManager,
		FireEffectManager* fireManager
	) {
		PROJECT_PROFILE_SCOPE("RestirManager::Dispatch");
		if (width_ == 0 || height_ == 0) return;

		int num_lights = lightManager.GetActiveLightCount();
		int num_fire = fireManager ? Constants::Class::Particles::MaxParticles() : 0;

		// Bind AllLights and Particles
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AllLights(), lightManager.GetAllLightsSsbo());
		if (fireManager) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), fireManager->GetParticleBuffer());
		}

		auto bind_textures = [&](ComputeShader* s) {
			s->use();
			s->setInt("u_num_lights", num_lights);
			s->setInt("u_num_fire_particles", num_fire);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTex);
			s->setInt("gDepth", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, normalTex);
			s->setInt("gNormal", 1);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, velocityTex);
			s->setInt("gVelocity", 2);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, colorTex);
			s->setInt("gColor", 3);

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, albedoTex);
			s->setInt("gAlbedo", 4);

			if (blueNoiseTex) {
				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseBlue());
				glBindTexture(GL_TEXTURE_2D, blueNoiseTex);
				s->setInt("u_blueNoiseTexture", Constants::TextureUnit::NoiseBlue());
			}

			if (permutation_tex_) {
				glActiveTexture(GL_TEXTURE8); // Use fixed unit 8 for permutation
				glBindTexture(GL_TEXTURE_2D, permutation_tex_);
				s->setInt("u_permutationTexture", 8);
			}
		};

		// 1. DI Initial Sampling
		bind_textures(di_sampling_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirReservoirs0(), reservoir_buffers_[current_buffer_index_]);
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 2. DI Temporal Reuse
		bind_textures(di_temporal_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirReservoirs0(), reservoir_buffers_[current_buffer_index_]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirReservoirs1(), reservoir_buffers_[1 - current_buffer_index_]);
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 3. DI Spatial Reuse
		// We must ping-pong here to avoid race conditions and directional line artifacts.
		// Temporal result is in reservoir_buffers_[current_buffer_index_].
		// We read from it and write the spatial result to reservoir_buffers_[1 - current_buffer_index_].
		bind_textures(di_spatial_shader_.get());

		// NewReservoirs (Binding 47) <- 1 - current
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirReservoirs0(), reservoir_buffers_[1 - current_buffer_index_]);
		// OldReservoirs (Binding 48) <- current
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirReservoirs1(), reservoir_buffers_[current_buffer_index_]);

		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 4. GI Trace
		bind_textures(gi_trace_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirGIReservoirs0(), gi_reservoir_buffers_[current_buffer_index_]);
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 5. GI Temporal Reuse
		bind_textures(gi_reuse_shader_.get());
		// Output to reservoirs0 (current), input from reservoirs1 (previous)
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirGIReservoirs0(), gi_reservoir_buffers_[current_buffer_index_]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirGIReservoirs1(), gi_reservoir_buffers_[1 - current_buffer_index_]);
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// 6. GI Spatial Reuse
		bind_textures(gi_spatial_shader_.get());
		// Input from reservoirs0 (temporal result), output to reservoirs1
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirGIReservoirs0(), gi_reservoir_buffers_[1 - current_buffer_index_]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::RestirGIReservoirs1(), gi_reservoir_buffers_[current_buffer_index_]);
		glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// Swap buffer index for next frame's temporal pass

		current_buffer_index_ = 1 - current_buffer_index_;
	}

} // namespace Boidsish
