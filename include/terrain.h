#pragma once

#include <memory>
#include <vector>

#include "field.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Shader;

namespace Boidsish {

	class TerrainRenderManager;

	/**
	 * @brief Represents a single terrain chunk with its geometry data.
	 *
	 * Terrain chunks are managed by TerrainGenerator and rendered via
	 * TerrainRenderManager. Unlike other renderable objects, Terrain
	 * does not inherit from Shape as it:
	 * - Is never rendered through the standard shape pipeline
	 * - Is never instanced via InstanceManager
	 * - Has no color, trail, rotation, or PBR properties
	 * - Only needs position and geometry data
	 */
	class Terrain {
	public:
		Terrain(
			const std::vector<unsigned int>& indices,
			const std::vector<glm::vec3>&    vertices,
			const std::vector<glm::vec3>&    normals,
			const PatchProxy&                proxy
		);
		~Terrain();

		// Position accessors
		float GetX() const { return x_; }

		float GetY() const { return y_; }

		float GetZ() const { return z_; }

		void SetPosition(float x, float y, float z) {
			x_ = x;
			y_ = y;
			z_ = z;
		}

		glm::mat4 GetModelMatrix() const {
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(x_, y_, z_));
			return model;
		}

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

	private:
		float x_ = 0.0f;
		float y_ = 0.0f;
		float z_ = 0.0f;

		std::vector<float>        vertex_data_; // Interleaved for GPU
		std::vector<unsigned int> indices_;
		int                       index_count_;
	};

} // namespace Boidsish
