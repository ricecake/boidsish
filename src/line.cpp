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
		Shape(id, start.x, start.y, start.z, r, g, b, a), end_(end), width_(width), style_(Style::SOLID) {}

	Line::Line(glm::vec3 start, glm::vec3 end, float width):
		Shape(0, start.x, start.y, start.z, 1.0f, 1.0f, 1.0f, 1.0f), end_(end), width_(width), style_(Style::SOLID) {}

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

			std::vector<float> buffer_data;
			for (const auto& tv : temp_verts) {
				buffer_data.push_back(tv.p.x);
				buffer_data.push_back(tv.p.y);
				buffer_data.push_back(tv.p.z);
				buffer_data.push_back(tv.t.x);
				buffer_data.push_back(tv.t.y);
			}

			glBufferData(GL_ARRAY_BUFFER, buffer_data.size() * sizeof(float), buffer_data.data(), GL_STATIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
			glEnableVertexAttribArray(2);

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}
	}

	void Line::DestroyLineMesh() {
		if (line_vao_ != 0) {
			glDeleteVertexArrays(1, &line_vao_);
			glDeleteBuffers(1, &line_vbo_);
			line_vao_ = 0;
			line_vbo_ = 0;
		}
	}

	void Line::render() const {
		if (shader) {
			render(*shader, GetModelMatrix());
		}
	}

	void Line::render(Shader& s, const glm::mat4& model_matrix) const {
		s.use();
		s.setMat4("model", model_matrix);

		// Use standard uniforms for color and alpha
		s.setVec3("objectColor", glm::vec3(GetR(), GetG(), GetB()));
		s.setFloat("objectAlpha", GetA());
		s.setBool("use_texture", false);

		// Set Line-specific uniforms
		s.setBool("isLine", true);
		s.setInt("lineStyle", static_cast<int>(style_));

		// Pass through isColossal from base class
		s.setBool("isColossal", IsColossal());

		glBindVertexArray(line_vao_);
		if (line_allocation_.valid) {
			glDrawArrays(GL_TRIANGLES, static_cast<GLint>(line_allocation_.base_vertex), line_vertex_count_);
		} else {
			glDrawArrays(GL_TRIANGLES, 0, line_vertex_count_);
		}
		glBindVertexArray(0);

		// Reset uniforms to avoid affecting other shapes
		s.setBool("isLine", false);
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
		if (line_vao_ == 0) {
			InitLineMesh(context.megabuffer);
		}

		glm::mat4 model_matrix = GetModelMatrix();
		glm::vec3 world_pos = (GetStart() + GetEnd()) * 0.5f;

		RenderPacket packet;
		packet.vao = line_vao_;
		if (line_allocation_.valid) {
			packet.base_vertex = line_allocation_.base_vertex;
		}
		packet.vertex_count = static_cast<unsigned int>(line_vertex_count_);
		packet.draw_mode = GL_TRIANGLES;
		packet.shader_id = shader ? shader->ID : 0;

		packet.uniforms.model = model_matrix;
		packet.uniforms.color = glm::vec4(GetR(), GetG(), GetB(), GetA());
		packet.uniforms.use_pbr = UsePBR();
		packet.uniforms.roughness = GetRoughness();
		packet.uniforms.metallic = GetMetallic();
		packet.uniforms.ao = GetAO();
		packet.uniforms.use_texture = false;
		packet.uniforms.is_colossal = IsColossal();

		packet.uniforms.is_line = 1;
		packet.uniforms.line_style = static_cast<int>(style_);

		packet.casts_shadows = CastsShadows();

		RenderLayer layer = IsTransparent() ? RenderLayer::Transparent : RenderLayer::Opaque;
		packet.shader_handle = shader_handle;
		packet.material_handle = MaterialHandle(0);

		float normalized_depth = context.CalculateNormalizedDepth(world_pos);
		packet.sort_key = CalculateSortKey(layer, packet.shader_handle, packet.material_handle, normalized_depth);

		out_packets.push_back(packet);
	}
} // namespace Boidsish
