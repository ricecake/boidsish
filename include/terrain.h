#pragma once

#include <vector>

#include "field.h"
#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class Terrain: public Shape {
	public:
		Terrain(
			const std::vector<float>&        vertexData,
			const std::vector<unsigned int>& indices,
			std::shared_ptr<Shader>          shader
		);
		~Terrain();

		void setupMesh();
		void render() const override;

	private:
		std::vector<float>        vertex_data_;
		std::vector<unsigned int> indices_;
		std::shared_ptr<Shader>   shader_;

		unsigned int vao_, vbo_, ebo_;
		int          index_count_;
	};

} // namespace Boidsish
