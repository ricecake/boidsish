#pragma once

#include <memory>
#include <vector>

#include "graphics.h"

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

		void render() const override;
	};

} // namespace Boidsish
