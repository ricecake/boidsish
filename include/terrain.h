#pragma once

#include <vector>

#include "field.h"
#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class TerrainRenderManager;

	class Terrain: public Shape {
	public:
		Terrain(
			const std::vector<unsigned int>& indices,
			const std::vector<glm::vec3>&    vertices,
			const std::vector<glm::vec3>&    normals,
			const PatchProxy&                proxy
		);
		~Terrain();

		// Legacy per-chunk GPU setup (deprecated - use TerrainRenderManager instead)
		void      setupMesh();
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix, const glm::mat4& prev_model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		// Terrain chunks are not instanced (each has unique geometry)
		std::string GetInstanceKey() const override { return "Terrain:" + std::to_string(GetId()); }

		static std::shared_ptr<Shader> terrain_shader_;

		// Public members for field calculations
		PatchProxy             proxy;
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;

		/**
		 * @brief Get interleaved vertex data for batched rendering.
		 *
		 * Format: [pos.x, pos.y, pos.z, normal.x, normal.y, normal.z, texcoord.u, texcoord.v] per vertex
		 *
		 * @return Interleaved vertex data
		 */
		std::vector<float> GetInterleavedVertexData() const;

		/**
		 * @brief Get index data for batched rendering.
		 *
		 * @return Index data
		 */
		const std::vector<unsigned int>& GetIndices() const { return indices_; }

		/**
		 * @brief Check if this chunk uses legacy per-chunk GPU resources.
		 */
		bool HasLegacyGPUResources() const { return vao_ != 0; }

		/**
		 * @brief Mark this chunk as managed by TerrainRenderManager (skip legacy GPU setup).
		 */
		void SetManagedByRenderManager(bool managed) { managed_by_render_manager_ = managed; }

		bool IsManagedByRenderManager() const { return managed_by_render_manager_; }

	private:
		std::vector<float>        vertex_data_; // Interleaved for GPU
		std::vector<unsigned int> indices_;

		unsigned int vao_ = 0, vbo_ = 0, ebo_ = 0;
		int          index_count_;

		bool managed_by_render_manager_ = false;
	};

} // namespace Boidsish
