#include "graph.h"

#include <cmath>
#include <map>
#include <numbers>
#include <vector>

#include "dot.h"
#include "shader.h"
#include "spline.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	Graph::Graph(int id, float x, float y, float z): Shape(id, x, y, z, 1.0f, 1.0f, 1.0f, 1.0f, 0) {}

	Graph::~Graph() {
		if (buffers_initialized_) {
			glDeleteVertexArrays(1, &graph_vao_);
			glDeleteBuffers(1, &graph_vbo_);
		}
	}

	const int   CYLINDER_SEGMENTS = 12;
	const float EDGE_RADIUS_SCALE = 0.005f;
	const int   CURVE_SEGMENTS = 10;

	void Graph::PrepareResources(Megabuffer* mb) const {
		if (edges.empty())
			return;

		bool nodes_changed = false;
		if (cached_vertex_positions_.size() != vertices.size()) {
			nodes_changed = true;
		} else {
			for (size_t i = 0; i < vertices.size(); ++i) {
				if ((vertices[i].position - cached_vertex_positions_[i]).MagnitudeSquared() > 1e-9) {
					nodes_changed = true;
					break;
				}
			}
		}

		if (mb) {
			if (allocation_.valid && !nodes_changed)
				return;

			std::vector<Spline::VertexData> all_vertices_data;

			std::map<int, std::vector<int>> adj;
			for (const auto& edge : edges) {
				adj[edge.from_vertex_index].push_back(edge.to_vertex_index);
				adj[edge.to_vertex_index].push_back(edge.from_vertex_index);
			}

			for (const auto& edge : edges) {
				if (edge.from_vertex_index >= (int)vertices.size() || edge.to_vertex_index >= (int)vertices.size())
					continue;

				const auto& v1 = vertices[edge.from_vertex_index];
				const auto& v2 = vertices[edge.to_vertex_index];

				Graph::Node v0 = v1;
				if (adj.count(edge.from_vertex_index)) {
					for (int n_idx : adj[edge.from_vertex_index]) {
						if (n_idx != edge.to_vertex_index) {
							v0 = vertices[n_idx];
							break;
						}
					}
				}
				if (v0.GetId() == v1.GetId())
					v0.position = v1.position - (v2.position - v1.position);

				Graph::Node v3 = v2;
				if (adj.count(edge.to_vertex_index)) {
					for (int n_idx : adj[edge.to_vertex_index]) {
						if (n_idx != edge.from_vertex_index) {
							v3 = vertices[n_idx];
							break;
						}
					}
				}
				if (v3.GetId() == v2.GetId())
					v3.position = v2.position + (v2.position - v1.position);

				Vector3 p0 = v0.position, p1 = v1.position, p2 = v2.position, p3 = v3.position;

				std::vector<std::vector<Spline::VertexData>> rings;
				Vector3                                      last_normal;

				{
					Vector3 point1 = Spline::CatmullRom(0.0f, p0, p1, p2, p3);
					Vector3 point2 = Spline::CatmullRom(1.0f / CURVE_SEGMENTS, p0, p1, p2, p3);
					Vector3 tangent;
					if ((point2 - point1).MagnitudeSquared() < 1e-6) {
						tangent = Vector3(0, 1, 0);
					} else {
						tangent = (point2 - point1).Normalized();
					}

					if (abs(tangent.y) < 0.999)
						last_normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
					else
						last_normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
				}

				for (int i = 0; i <= CURVE_SEGMENTS; ++i) {
					std::vector<Spline::VertexData> ring;
					float                           t = (float)i / CURVE_SEGMENTS;

					Vector3   point = Spline::CatmullRom(t, p0, p1, p2, p3);
					glm::vec3 color = {(1 - t) * v1.r + t * v2.r, (1 - t) * v1.g + t * v2.g, (1 - t) * v1.b + t * v2.b};
					float     r = ((1 - t) * v1.size + t * v2.size) * EDGE_RADIUS_SCALE;

					Vector3 tangent;
					if (i < CURVE_SEGMENTS) {
						Vector3 next_point = Spline::CatmullRom((float)(i + 1) / CURVE_SEGMENTS, p0, p1, p2, p3);
						if ((next_point - point).MagnitudeSquared() < 1e-6) {
							tangent = Vector3(0, 1, 0);
						} else {
							tangent = (next_point - point).Normalized();
						}
					} else {
						Vector3 prev_point = Spline::CatmullRom((float)(i - 1) / CURVE_SEGMENTS, p0, p1, p2, p3);
						if ((point - prev_point).MagnitudeSquared() < 1e-6) {
							tangent = Vector3(0, 1, 0);
						} else {
							tangent = (point - prev_point).Normalized();
						}
					}

					Vector3 normal = last_normal - tangent * tangent.Dot(last_normal);
					if (normal.MagnitudeSquared() < 1e-6) {
						if (abs(tangent.y) < 0.999)
							normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
						else
							normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
					} else {
						normal.Normalize();
					}
					Vector3 bitangent = tangent.Cross(normal).Normalized();
					last_normal = normal;

					for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
						float   angle = 2.0f * std::numbers::pi * j / CYLINDER_SEGMENTS;
						Vector3 cn = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
						Vector3 pos = point + cn * r;
						ring.push_back({glm::vec3(pos.x, pos.y, pos.z), glm::vec3(cn.x, cn.y, cn.z), color});
					}
					rings.push_back(ring);
				}

				for (int i = 0; i < CURVE_SEGMENTS; ++i) {
					for (int j = 0; j < CYLINDER_SEGMENTS; ++j) {
						all_vertices_data.push_back(rings[i][j]);
						all_vertices_data.push_back(rings[i][j + 1]);
						all_vertices_data.push_back(rings[i + 1][j]);

						all_vertices_data.push_back(rings[i][j + 1]);
						all_vertices_data.push_back(rings[i + 1][j + 1]);
						all_vertices_data.push_back(rings[i + 1][j]);
					}
				}
			}

			edge_vertex_count_ = static_cast<int>(all_vertices_data.size());
			std::vector<::Boidsish::Vertex> vertices_to_upload;
			vertices_to_upload.reserve(all_vertices_data.size());
			for (const auto& vd : all_vertices_data) {
				::Boidsish::Vertex v;
				v.Position = vd.pos;
				v.Normal = vd.normal;
				v.TexCoords = {0, 0};
				v.Color = vd.color;
				vertices_to_upload.push_back(v);
			}

			allocation_ = mb->AllocateStatic(vertices_to_upload.size(), 0);
			if (allocation_.valid) {
				mb->Upload(allocation_, vertices_to_upload.data(), vertices_to_upload.size());
				graph_vao_ = mb->GetVAO();
			}

			cached_vertex_positions_.clear();
			for (const auto& node : vertices)
				cached_vertex_positions_.push_back(node.position);
			buffers_initialized_ = true;
		} else {
			SetupBuffers();
		}
	}

	void Graph::SetupBuffers() const {
		if (edges.empty())
			return;

		std::vector<Spline::VertexData> all_vertices_data;

		std::map<int, std::vector<int>> adj;
		for (const auto& edge : edges) {
			adj[edge.from_vertex_index].push_back(edge.to_vertex_index);
			adj[edge.to_vertex_index].push_back(edge.from_vertex_index);
		}

		for (const auto& edge : edges) {
			if (edge.from_vertex_index >= (int)vertices.size() || edge.to_vertex_index >= (int)vertices.size())
				continue;

			const auto& v1 = vertices[edge.from_vertex_index];
			const auto& v2 = vertices[edge.to_vertex_index];

			Graph::Node v0 = v1;
			if (adj.count(edge.from_vertex_index)) {
				for (int n_idx : adj[edge.from_vertex_index]) {
					if (n_idx != edge.to_vertex_index) {
						v0 = vertices[n_idx];
						break;
					}
				}
			}
			if (v0.GetId() == v1.GetId())
				v0.position = v1.position - (v2.position - v1.position);

			Graph::Node v3 = v2;
			if (adj.count(edge.to_vertex_index)) {
				for (int n_idx : adj[edge.to_vertex_index]) {
					if (n_idx != edge.from_vertex_index) {
						v3 = vertices[n_idx];
						break;
					}
				}
			}
			if (v3.GetId() == v2.GetId())
				v3.position = v2.position + (v2.position - v1.position);

			Vector3 p0 = v0.position, p1 = v1.position, p2 = v2.position, p3 = v3.position;

			std::vector<std::vector<Spline::VertexData>> rings;
			Vector3                                      last_normal;

			{
				Vector3 point1 = Spline::CatmullRom(0.0f, p0, p1, p2, p3);
				Vector3 point2 = Spline::CatmullRom(1.0f / CURVE_SEGMENTS, p0, p1, p2, p3);
				Vector3 tangent;
				if ((point2 - point1).MagnitudeSquared() < 1e-6) {
					tangent = Vector3(0, 1, 0);
				} else {
					tangent = (point2 - point1).Normalized();
				}

				if (abs(tangent.y) < 0.999)
					last_normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
				else
					last_normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
			}

			for (int i = 0; i <= CURVE_SEGMENTS; ++i) {
				std::vector<Spline::VertexData> ring;
				float                           t = (float)i / CURVE_SEGMENTS;

				Vector3   point = Spline::CatmullRom(t, p0, p1, p2, p3);
				glm::vec3 color = {(1 - t) * v1.r + t * v2.r, (1 - t) * v1.g + t * v2.g, (1 - t) * v1.b + t * v2.b};
				float     r = ((1 - t) * v1.size + t * v2.size) * EDGE_RADIUS_SCALE;

				Vector3 tangent;
				if (i < CURVE_SEGMENTS) {
					Vector3 next_point = Spline::CatmullRom((float)(i + 1) / CURVE_SEGMENTS, p0, p1, p2, p3);
					if ((next_point - point).MagnitudeSquared() < 1e-6) {
						tangent = Vector3(0, 1, 0);
					} else {
						tangent = (next_point - point).Normalized();
					}
				} else {
					Vector3 prev_point = Spline::CatmullRom((float)(i - 1) / CURVE_SEGMENTS, p0, p1, p2, p3);
					if ((point - prev_point).MagnitudeSquared() < 1e-6) {
						tangent = Vector3(0, 1, 0);
					} else {
						tangent = (point - prev_point).Normalized();
					}
				}

				Vector3 normal = last_normal - tangent * tangent.Dot(last_normal);
				if (normal.MagnitudeSquared() < 1e-6) {
					if (abs(tangent.y) < 0.999)
						normal = tangent.Cross(Vector3(0, 1, 0)).Normalized();
					else
						normal = tangent.Cross(Vector3(1, 0, 0)).Normalized();
				} else {
					normal.Normalize();
				}
				Vector3 bitangent = tangent.Cross(normal).Normalized();
				last_normal = normal;

				for (int j = 0; j <= CYLINDER_SEGMENTS; ++j) {
					float   angle = 2.0f * std::numbers::pi * j / CYLINDER_SEGMENTS;
					Vector3 cn = (normal * cos(angle) + bitangent * sin(angle)).Normalized();
					Vector3 pos = point + cn * r;
					ring.push_back({glm::vec3(pos.x, pos.y, pos.z), glm::vec3(cn.x, cn.y, cn.z), color});
				}
				rings.push_back(ring);
			}

			for (int i = 0; i < CURVE_SEGMENTS; ++i) {
				for (int j = 0; j < CYLINDER_SEGMENTS; ++j) {
					all_vertices_data.push_back(rings[i][j]);
					all_vertices_data.push_back(rings[i][j + 1]);
					all_vertices_data.push_back(rings[i + 1][j]);

					all_vertices_data.push_back(rings[i][j + 1]);
					all_vertices_data.push_back(rings[i + 1][j + 1]);
					all_vertices_data.push_back(rings[i + 1][j]);
				}
			}
		}

		edge_vertex_count_ = all_vertices_data.size();

		std::vector<::Boidsish::Vertex> vertices_to_upload;
		vertices_to_upload.reserve(all_vertices_data.size());
		for (const auto& vd : all_vertices_data) {
			::Boidsish::Vertex v;
			v.Position = vd.pos;
			v.Normal = vd.normal;
			v.TexCoords = {0, 0};
			v.Color = vd.color;
			vertices_to_upload.push_back(v);
		}

		if (graph_vao_ == 0)
			glGenVertexArrays(1, &graph_vao_);
		glBindVertexArray(graph_vao_);
		if (graph_vbo_ == 0)
			glGenBuffers(1, &graph_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, graph_vbo_);
		glBufferData(
			GL_ARRAY_BUFFER,
			vertices_to_upload.size() * sizeof(::Boidsish::Vertex),
			vertices_to_upload.data(),
			GL_STATIC_DRAW
		);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(::Boidsish::Vertex), (void*)offsetof(::Boidsish::Vertex, Position));
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(::Boidsish::Vertex), (void*)offsetof(::Boidsish::Vertex, Normal));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(::Boidsish::Vertex), (void*)offsetof(::Boidsish::Vertex, TexCoords));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(::Boidsish::Vertex), (void*)offsetof(::Boidsish::Vertex, Color));
		glEnableVertexAttribArray(8);

		glBindVertexArray(0);
		buffers_initialized_ = true;
		cached_vertex_positions_.clear();
		cached_vertex_positions_.reserve(vertices.size());
		for (const auto& node : vertices) {
			cached_vertex_positions_.push_back(node.position);
		}
	}

	void Graph::render() const {
		if (cached_vertex_positions_.size() != vertices.size()) {
			buffers_initialized_ = false;
		} else {
			for (size_t i = 0; i < vertices.size(); ++i) {
				if ((vertices[i].position - cached_vertex_positions_[i]).MagnitudeSquared() > 1e-9) {
					buffers_initialized_ = false;
					break;
				}
			}
		}

		if (!buffers_initialized_) {
			SetupBuffers();
		}

		for (const auto& node : vertices) {
			Dot(0,
			    node.position.x + GetX(),
			    node.position.y + GetY(),
			    node.position.z + GetZ(),
			    node.size,
			    node.r,
			    node.g,
			    node.b,
			    node.a,
			    0)
				.render();
		}

		render(*shader, GetModelMatrix());

		// Reset model matrix for subsequent renders if needed
		glm::mat4 model = glm::mat4(1.0f);
		shader->setMat4("model", model);
	}

	void Graph::render(Shader& shader, const glm::mat4& model_matrix) const {
		if (!buffers_initialized_) {
			SetupBuffers();
		}

		shader.setMat4("model", model_matrix);
		shader.setInt("useVertexColor", 1);

		glBindVertexArray(graph_vao_);
		if (allocation_.valid) {
			glDrawArrays(GL_TRIANGLES, static_cast<GLint>(allocation_.base_vertex), edge_vertex_count_);
		} else {
			glDrawArrays(GL_TRIANGLES, 0, edge_vertex_count_);
		}
		glBindVertexArray(0);

		shader.setInt("useVertexColor", 0);
	}

	glm::mat4 Graph::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		// Graph doesn't have its own rotation or scale, so we don't apply them here.
		return model;
	}

} // namespace Boidsish

namespace Boidsish {
	void Graph::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		if (edges.empty() && vertices.empty())
			return;

		// 1. Generate packets for node dots
		for (const auto& node : vertices) {
			Dot dot(0,
			        node.position.x + GetX(),
			        node.position.y + GetY(),
			        node.position.z + GetZ(),
			        node.size,
			        node.r,
			        node.g,
			        node.b,
			        node.a,
			        0);
			dot.SetUsePBR(UsePBR());
			dot.SetRoughness(GetRoughness());
			dot.SetMetallic(GetMetallic());
			dot.SetAO(GetAO());
			dot.GenerateRenderPackets(out_packets, context);
		}

		if (edges.empty())
			return;

		glm::mat4 model_matrix = GetModelMatrix();
		glm::vec3 world_pos = glm::vec3(GetX(), GetY(), GetZ());

		RenderPacket packet;
		packet.vao = graph_vao_;
		if (allocation_.valid) {
			packet.base_vertex = allocation_.base_vertex;
		}
		packet.vertex_count = static_cast<unsigned int>(edge_vertex_count_);
		packet.draw_mode = GL_TRIANGLES;
		packet.shader_id = shader ? shader->ID : 0;

		packet.uniforms.model = model_matrix;
		packet.uniforms.color = glm::vec4(GetR(), GetG(), GetB(), GetA());
		packet.uniforms.use_pbr = UsePBR();
		packet.uniforms.roughness = GetRoughness();
		packet.uniforms.metallic = GetMetallic();
		packet.uniforms.ao = GetAO();
		packet.uniforms.use_texture = 0;
		packet.uniforms.use_vertex_color = 1; // Graph uses vertex colors
		packet.uniforms.is_colossal = IsColossal();

		packet.casts_shadows = CastsShadows();

		RenderLayer layer = (GetA() < 0.99f) ? RenderLayer::Transparent : RenderLayer::Opaque;

		packet.shader_handle = shader_handle;
		packet.material_handle = MaterialHandle(0);

		float normalized_depth = context.CalculateNormalizedDepth(world_pos);
		packet.sort_key = CalculateSortKey(
			layer,
			packet.shader_handle,
			packet.vao,
			packet.draw_mode,
			packet.index_count > 0,
			packet.material_handle,
			normalized_depth
		);

		out_packets.push_back(packet);
	}
} // namespace Boidsish
