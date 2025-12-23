#pragma once

#include <vector>

#include "field.h"
#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {
	class Terrain: public Shape {
	public:
		Terrain(const std::vector<float>& vertexData, const std::vector<unsigned int>& indices);
		~Terrain();

		void render() const override;

		const std::vector<glm::vec3>& getVertices() const { return vertices_; }
		const std::vector<glm::vec3>& getNormals() const { return normals_; }

		static std::shared_ptr<Shader> terrain_shader_;

	private:
        friend class TerrainGenerator;
		std::vector<glm::vec3> vertices_;
		std::vector<glm::vec3> normals_;
		void setupMesh(const std::vector<float>& vertexData, const std::vector<unsigned int>& indices);

		unsigned int vao_, vbo_, ebo_;
		int          index_count_;
	};

} // namespace Boidsish
