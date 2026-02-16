#pragma once

#include <memory>
#include <vector>

#include "shape.h"
#include "vector.h"
#include <GL/glew.h>

namespace Boidsish {

	class Graph: public Shape, public std::enable_shared_from_this<Graph> {
	public:
		struct Vertex {
			Vector3 position;
			float   size;
			float   r, g, b, a;

			Vertex Link(Vertex& other) {
				if (auto tmp = parent_.lock()) {
					tmp->AddEdge(*this, other);
				}
				return other;
			}

			constexpr int GetId() const { return id; }
			friend class Graph;

			Vertex(std::weak_ptr<Graph> parent, int Id): id(Id), parent_(parent) {}

			Vertex(
				std::weak_ptr<Graph> parent,
				int                  Id,
				const Vector3&       Pos,
				float                Size = 0,
				float                R = 0,
				float                G = 0,
				float                B = 0,
				float                A = 0
			):
				position(Pos), size(Size), r(R), g(G), b(B), a(A), id(Id), parent_(parent) {}

		private:
			int                  id = -1;
			std::weak_ptr<Graph> parent_;
		};

		struct Edge {
			int from_vertex_index;
			int to_vertex_index;

			constexpr int GetId() const { return id; }

			friend class Graph;

			Edge(std::weak_ptr<Graph> parent, int Id): id(Id), parent_(parent) {}

			Edge(std::weak_ptr<Graph> parent, int Id, int L, int R):
				from_vertex_index(L), to_vertex_index(R), id(Id), parent_(parent) {}

			Edge(std::weak_ptr<Graph> parent, int Id, const Vertex& L, const Vertex& R):
				from_vertex_index(L.GetId()), to_vertex_index(R.GetId()), id(Id), parent_(parent) {}

		private:
			int                  id;
			std::weak_ptr<Graph> parent_;
		};

		Graph(int id = 0, float x = 0.0f, float y = 0.0f, float z = 0.0f);
		~Graph();

		void      SetupBuffers() const;
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		void GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const override;

		// Graphs are not instanced (each has unique geometry)
		std::string GetInstanceKey() const override { return "Graph:" + std::to_string(GetId()); }

		Vertex& AddVertex(const Vector3& pos, float Size = 0, float R = 0, float G = 0, float B = 0, float A = 0) {
			buffers_initialized_ = false;
			return vertices.emplace_back(weak_from_this(), static_cast<int>(vertices.size()), pos, Size, R, G, B, A);
		}

		Edge& AddEdge(const Vertex& a, const Vertex& b) {
			buffers_initialized_ = false;
			// 1. Create the new Edge object
			return edges.emplace_back(weak_from_this(), static_cast<int>(edges.size()), a, b);
		}

		Edge& E(int id) { return edges.at(id); }

		Vertex& V(int id) { return vertices.at(id); }

	private:
		std::vector<Vertex> vertices;
		std::vector<Edge>   edges;

		mutable GLuint               graph_vao_ = 0;
		mutable GLuint               graph_vbo_ = 0;
		mutable int                  edge_vertex_count_ = 0;
		mutable bool                 buffers_initialized_ = false;
		mutable std::vector<Vector3> cached_vertex_positions_;
	};

} // namespace Boidsish
