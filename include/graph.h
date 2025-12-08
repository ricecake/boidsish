#pragma once

#include <memory>
#include <vector>

#include "boidsish.h"
#include <glm/mat4x4.hpp>

class Shader;

namespace Boidsish {

	class Graph: public Shape {
	public:
		struct Vertex {
			Vector3 position;
			float   size;
			float   r, g, b, a;
		};

		struct Edge {
			int vertex1_idx;
			int vertex2_idx;
		};

		std::vector<Vertex> vertices;
		std::vector<Edge>   edges;

		Graph(int id = 0, float x = 0.0f, float y = 0.0f, float z = 0.0f);
		~Graph();

		void render() const override;
		void render(Shader& sphere_shader,
		            Shader& edge_shader,
		            unsigned int sphere_VAO,
		            int sphere_index_count,
		            const glm::mat4& projection,
		            const glm::mat4& view) const;

	private:
		mutable unsigned int edge_VAO = 0;
		mutable unsigned int edge_VBO = 0;
		mutable size_t       edge_vertex_count = 0;
		mutable bool         buffers_need_update = true;

		void updateEdgeBuffers(const Vector3& offset) const;
	};

} // namespace Boidsish
