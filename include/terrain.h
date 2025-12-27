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
			std::shared_ptr<Shader>          shader
		);
		~Terrain();

		void setupMesh();
		void render() const override;

		// Public members for field calculations
		PatchProxy             proxy;
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;

	private:
		std::shared_ptr<Shader>   shader_;
		std::vector<float>        vertex_data_; // Interleaved for GPU
		std::vector<unsigned int> indices_;

		unsigned int vao_, vbo_, ebo_;
		int          index_count_;
	};

} // namespace Boidsish
