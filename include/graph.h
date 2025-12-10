#pragma once

#include <memory>
#include <vector>

#include "graphics.h"
#include <GL/glew.h>

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

		void setup_buffers() const;
		void render(Shader& shader) const;
		void render() const override;

	private:
		mutable GLuint vao = 0;
		mutable GLuint vbo = 0;
		mutable int    edge_vertex_count = 0;
		mutable bool   buffers_initialized = false;
	};

} // namespace Boidsish
