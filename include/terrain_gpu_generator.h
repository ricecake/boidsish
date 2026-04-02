#pragma once

#include <map>
#include <memory>
#include <vector>
#include <queue>
#include <optional>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <utility>
#include <shader.h>

namespace Boidsish {

	class TerrainRenderManager;

	/**
	 * @brief Handles GPU-side terrain generation using compute shaders.
	 * Manages SSBO pooling and non-blocking read-backs with fences.
	 */
	class TerrainGPUGenerator {
	public:
		struct GPUResult {
			std::vector<glm::vec3> positions;
			std::vector<glm::vec3> normals;
			std::vector<glm::vec2> biomes;
			int                    chunkX;
			int                    chunkZ;
			int                    slice;
		};

		TerrainGPUGenerator(int chunk_size);
		~TerrainGPUGenerator();

		/**
		 * @brief Request a chunk to be generated on the GPU.
		 * @param chunk_key (x, z) coordinates
		 * @param slice Texture array slice to write to
		 * @param world_offset World-space offset of the chunk
		 * @param world_scale Global world scale
		 */
		void RequestChunk(
			std::pair<int, int> chunk_key,
			int                 slice,
			const glm::vec3&    world_offset,
			float               world_scale
		);

		/**
		 * @brief Poll for completed GPU generation results.
		 * Non-blocking.
		 * @param chunk_key The chunk to check
		 * @return Result if ready, otherwise nullopt
		 */
		std::optional<GPUResult> TryGetResult(std::pair<int, int> chunk_key);

		/**
		 * @brief Cancel a pending GPU generation task.
		 */
		void CancelRequest(std::pair<int, int> chunk_key);

		/**
		 * @brief Set the render manager to access shared textures and UBOs.
		 */
		void SetRenderManager(std::shared_ptr<TerrainRenderManager> manager) { render_manager_ = manager; }

	private:
		struct InFlightChunk {
			std::pair<int, int> chunk_key;
			int                 slice;
			glm::vec3           world_offset;
			float               world_scale;
			GLuint              ssbos[3]; // pos, norm, biome
			GLsync              fence;
		};

		struct SSBOContainer {
			GLuint ssbos[3];
		};

		int                                   chunk_size_;
		int                                   num_vertices_;
		std::shared_ptr<TerrainRenderManager> render_manager_;
		std::unique_ptr<ComputeShader>        gen_shader_;
		GLuint                                simplex_ubo_ = 0;

		std::map<std::pair<int, int>, InFlightChunk> in_flight_chunks_;
		std::queue<SSBOContainer>                    ssbo_pool_;

		void   InitializeSimplexUBO();
		GLuint CreateSSBO(size_t size);
		SSBOContainer GetOrCreateSSBOs();
		void          ReturnSSBOs(const SSBOContainer& container);
	};

} // namespace Boidsish
