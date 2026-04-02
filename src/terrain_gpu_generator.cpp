#include "terrain_gpu_generator.h"

#include <iostream>

#include "Simplex.h"
#include "constants.h"
#include "shader.h"
#include "terrain_render_manager.h"

namespace Boidsish {

	TerrainGPUGenerator::TerrainGPUGenerator(int chunk_size):
		chunk_size_(chunk_size), num_vertices_((chunk_size + 1) * (chunk_size + 1)) {
		gen_shader_ = std::make_unique<ComputeShader>("shaders/terrain_gen.comp");
		InitializeSimplexUBO();
	}

	TerrainGPUGenerator::~TerrainGPUGenerator() {
		for (auto& [key, chunk] : in_flight_chunks_) {
			if (chunk.fence) {
				glDeleteSync(chunk.fence);
			}
			glDeleteBuffers(3, chunk.ssbos);
		}

		while (!ssbo_pool_.empty()) {
			SSBOContainer container = ssbo_pool_.front();
			glDeleteBuffers(3, container.ssbos);
			ssbo_pool_.pop();
		}

		if (simplex_ubo_) {
			glDeleteBuffers(1, &simplex_ubo_);
		}
	}

	void TerrainGPUGenerator::InitializeSimplexUBO() {
		glGenBuffers(1, &simplex_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, simplex_ubo_);

		// Port of Simplex permutation table to UBO (ivec4[128])
		std::vector<int32_t> packed_perm(512);
		for (int i = 0; i < 512; ++i) {
			packed_perm[i] = static_cast<int32_t>(Simplex::details::perm[i]);
		}

		glBufferData(GL_UNIFORM_BUFFER, 512 * sizeof(int32_t), packed_perm.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	GLuint TerrainGPUGenerator::CreateSSBO(size_t size) {
		GLuint ssbo;
		glGenBuffers(1, &ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_STREAM_READ);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		return ssbo;
	}

	TerrainGPUGenerator::SSBOContainer TerrainGPUGenerator::GetOrCreateSSBOs() {
		if (!ssbo_pool_.empty()) {
			SSBOContainer container = ssbo_pool_.front();
			ssbo_pool_.pop();
			return container;
		}

		SSBOContainer container;
		container.ssbos[0] = CreateSSBO(num_vertices_ * 3 * sizeof(float)); // pos
		container.ssbos[1] = CreateSSBO(num_vertices_ * 3 * sizeof(float)); // norm
		container.ssbos[2] = CreateSSBO(num_vertices_ * 2 * sizeof(float)); // biome
		return container;
	}

	void TerrainGPUGenerator::ReturnSSBOs(const SSBOContainer& container) {
		ssbo_pool_.push(container);
	}

	void TerrainGPUGenerator::RequestChunk(
		std::pair<int, int> chunk_key,
		int                 slice,
		const glm::vec3&    world_offset,
		float               world_scale
	) {
		if (!render_manager_ || !gen_shader_ || !gen_shader_->isValid())
			return;

		SSBOContainer container = GetOrCreateSSBOs();

		gen_shader_->use();
		gen_shader_->setInt("u_chunkX", chunk_key.first);
		gen_shader_->setInt("u_chunkZ", chunk_key.second);
		gen_shader_->setFloat("u_worldScale", world_scale);
		gen_shader_->setInt("u_chunkSize", chunk_size_);
		gen_shader_->setInt("u_slice", slice);

		// Bind resources
		render_manager_->BindTerrainData(*gen_shader_);
		render_manager_->BindTerrainImages(0, 1);

		glBindBufferBase(GL_UNIFORM_BUFFER, 10, simplex_ubo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, container.ssbos[0]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, container.ssbos[1]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, container.ssbos[2]);

		uint32_t groups = (chunk_size_ + 1 + 7) / 8;
		glDispatchCompute(groups, groups, 1);

		InFlightChunk in_flight;
		in_flight.chunk_key = chunk_key;
		in_flight.slice = slice;
		in_flight.world_offset = world_offset;
		in_flight.world_scale = world_scale;
		in_flight.ssbos[0] = container.ssbos[0];
		in_flight.ssbos[1] = container.ssbos[1];
		in_flight.ssbos[2] = container.ssbos[2];
		in_flight.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

		in_flight_chunks_[chunk_key] = in_flight;

		// Ensure commands are submitted
		glFlush();
	}

	std::optional<TerrainGPUGenerator::GPUResult> TerrainGPUGenerator::TryGetResult(std::pair<int, int> chunk_key) {
		auto it = in_flight_chunks_.find(chunk_key);
		if (it == in_flight_chunks_.end()) {
			return std::nullopt;
		}

		InFlightChunk& in_flight = it->second;

		GLenum res = glClientWaitSync(in_flight.fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
		if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED) {
			GPUResult result;
			result.chunkX = it->first.first;
			result.chunkZ = it->first.second;
			result.slice = in_flight.slice;
			result.positions.resize(num_vertices_);
			result.normals.resize(num_vertices_);
			result.biomes.resize(num_vertices_);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, in_flight.ssbos[0]);
			glGetBufferSubData(
				GL_SHADER_STORAGE_BUFFER,
				0,
				num_vertices_ * 3 * sizeof(float),
				result.positions.data()
			);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, in_flight.ssbos[1]);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, num_vertices_ * 3 * sizeof(float), result.normals.data());

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, in_flight.ssbos[2]);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, num_vertices_ * 2 * sizeof(float), result.biomes.data());

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			glDeleteSync(in_flight.fence);
			ReturnSSBOs({in_flight.ssbos[0], in_flight.ssbos[1], in_flight.ssbos[2]});
			in_flight_chunks_.erase(it);

			return result;
		}

		return std::nullopt;
	}

	void TerrainGPUGenerator::CancelRequest(std::pair<int, int> chunk_key) {
		auto it = in_flight_chunks_.find(chunk_key);
		if (it != in_flight_chunks_.end()) {
			glDeleteSync(it->second.fence);
			ReturnSSBOs({it->second.ssbos[0], it->second.ssbos[1], it->second.ssbos[2]});
			in_flight_chunks_.erase(it);
		}
	}

} // namespace Boidsish
