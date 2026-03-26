#include "line.h"

#include <vector>

#include "shader.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	unsigned int         Line::line_vao_ = 0;
	unsigned int         Line::line_vbo_ = 0;
	int                  Line::line_vertex_count_ = 0;
	MegabufferAllocation Line::line_allocation_;

	Line::Line(int id, glm::vec3 start, glm::vec3 end, float width, float r, float g, float b, float a):
		Shape(id, start.x, start.y, start.z, 1.0f, r, g, b, a), end_(end), width_(width), style_(Style::SOLID) {}

	Line::Line(glm::vec3 start, glm::vec3 end, float width):
		Shape(0, start.x, start.y, start.z, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f),
		end_(end),
		width_(width),
		style_(Style::SOLID) {}

	void Line::InitLineMesh(Megabuffer* mb) {
		if (line_vao_ != 0 && line_allocation_.valid)
			return;

		// Create a mesh of two crossed quads to give some 3D volume
		struct TempVertex {
			glm::vec3 p;
			glm::vec2 t;
		};

		std::vector<TempVertex> temp_verts = {
			// Quad 1 (XY plane)
			{{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f}},
			{{1.0f, -0.5f, 0.0f}, {1.0f, 0.0f}},
			{{1.0f, 0.5f, 0.0f}, {1.0f, 1.0f}},
			{{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f}},
			{{1.0f, 0.5f, 0.0f}, {1.0f, 1.0f}},
			{{0.0f, 0.5f, 0.0f}, {0.0f, 1.0f}},

			// Quad 2 (XZ plane)
			{{0.0f, 0.0f, -0.5f}, {0.0f, 0.0f}},
			{{1.0f, 0.0f, -0.5f}, {1.0f, 0.0f}},
			{{1.0f, 0.0f, 0.5f}, {1.0f, 1.0f}},
			{{0.0f, 0.0f, -0.5f}, {0.0f, 0.0f}},
			{{1.0f, 0.0f, 0.5f}, {1.0f, 1.0f}},
			{{0.0f, 0.0f, 0.5f}, {0.0f, 1.0f}}
		};

		line_vertex_count_ = 12;

		if (mb) {
			std::vector<Vertex> vertices;
			for (const auto& tv : temp_verts) {
				Vertex v;
				v.Position = tv.p;
				v.Normal = glm::vec3(0, 1, 0); // Simplified normal
				v.TexCoords = tv.t;
				v.Color = glm::vec3(1, 1, 1);
				vertices.push_back(v);
			}

			line_allocation_ = mb->AllocateStatic(vertices.size(), 0);
			if (line_allocation_.valid) {
				mb->Upload(line_allocation_, vertices.data(), vertices.size());
				line_vao_ = mb->GetVAO();
			}
		} else {
			glGenVertexArrays(1, &line_vao_);
			glGenBuffers(1, &line_vbo_);

			glBindVertexArray(line_vao_);
			glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);

			std::vector<Vertex> vertices;
			for (const auto& tv : temp_verts) {
				Vertex v;
				v.Position = tv.p;
				v.Normal = glm::vec3(0, 1, 0); // Simplified normal
				v.TexCoords = tv.t;
				v.Color = glm::vec3(1, 1, 1);
				vertices.push_back(v);
			}

			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));
			glEnableVertexAttribArray(8);

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}
	}

	MeshInfo Line::GetMeshInfo(Megabuffer* mb) const {
		if (line_vao_ == 0) {
			InitLineMesh(mb);
		}
		MeshInfo info;
		info.vao = line_vao_;
		info.vbo = line_vbo_;
		info.vertex_count = static_cast<unsigned int>(line_vertex_count_);
		info.allocation = line_allocation_;
		return info;
	}

	void Line::DestroyLineMesh() {
		if (line_vao_ != 0) {
			glDeleteVertexArrays(1, &line_vao_);
			glDeleteBuffers(1, &line_vbo_);
			line_vao_ = 0;
			line_vbo_ = 0;
		}
	}


	glm::mat4 Line::GetModelMatrix() const {
		glm::vec3 start = GetStart();
		glm::vec3 direction = end_ - start;
		float     length = glm::length(direction);

		if (length < 0.0001f) {
			return glm::mat4(1.0f);
		}

		glm::vec3 norm_dir = direction / length;

		// Calculate rotation to align X-axis (our line mesh grows along X) with direction
		glm::quat rotation = glm::rotation(glm::vec3(1.0f, 0.0f, 0.0f), norm_dir);

		glm::mat4 model = glm::translate(glm::mat4(1.0f), start);
		model *= glm::toMat4(rotation);
		model = glm::scale(model, glm::vec3(length, width_, width_));

		return model;
	}

	void Line::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		MeshInfo mesh = GetMeshInfo(context.megabuffer);
		if (mesh.vao == 0)
			return;

		RenderPacket packet;
		PopulatePacket(packet, mesh, context);

		packet.uniforms.is_line = 1;
		packet.uniforms.line_style = static_cast<int>(style_);

		out_packets.push_back(std::move(packet));
	}
} // namespace Boidsish
