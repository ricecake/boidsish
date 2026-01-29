#pragma once

#include <vector>

#include "field.h"
#include "shape.h"
#include "Octree.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class Terrain: public Shape {
	public:
		Terrain(
			const std::vector<unsigned int>& indices,
			const std::vector<glm::vec3>&    vertices,
			const std::vector<glm::vec3>&    normals,
			const PatchProxy&                proxy
		);
		~Terrain();

		void      setupMesh();
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		static std::shared_ptr<Shader> terrain_shader_;

		bool raycast(const Octree<size_t>::Ray& ray, float& out_dist, glm::vec3& out_normal) const;

		// Public members for field calculations
		PatchProxy             proxy;
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;

	private:
		std::vector<float>        vertex_data_; // Interleaved for GPU
		std::vector<unsigned int> indices_;

		std::unique_ptr<Octree<size_t>> octree_;

		unsigned int vao_, vbo_, ebo_;
		int          index_count_;
	};

} // namespace Boidsish
