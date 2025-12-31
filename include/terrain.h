#pragma once

#include <vector>

#include "field.h"
#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class Terrain: public Shape {
	public:
		Terrain(
			const std::vector<unsigned int>& indices,
			const std::vector<glm::vec3>&    vertices,
			const std::vector<glm::vec3>&    normals,
			const PatchProxy&                proxy,
            int chunk_x,
            int chunk_z
		);
		~Terrain();

		void      setupMesh();
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		static std::shared_ptr<Shader> terrain_shader_;

		// Public members for field calculations
		PatchProxy             proxy;
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;
        int chunk_x;
        int chunk_z;

	private:
		std::vector<float>        vertex_data_; // Interleaved for GPU
		std::vector<unsigned int> indices_;

		unsigned int vao_, vbo_, ebo_;
		int          index_count_;
	};

} // namespace Boidsish
