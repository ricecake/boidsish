#include "arrow.h"

#include <cmath>
#include <vector>

#include "shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	Arrow::Arrow(
		int   id,
		float x,
		float y,
		float z,
		float cone_height,
		float cone_radius,
		float rod_radius,
		float r,
		float g,
		float b,
		float a
	):
		Shape(id, x, y, z, r, g, b, a), cone_height_(cone_height), cone_radius_(cone_radius), rod_radius_(rod_radius) {
		InitArrowMesh(nullptr);
	}

	void Arrow::PrepareResources(Megabuffer* mb) const {
		if (mb) {
			InitArrowMesh(mb);
		}
	}

	Arrow::~Arrow() {
		DestroyArrowMesh();
	}

	void Arrow::render() const {
		shader->use();
		render(*shader, GetModelMatrix());
	}

	void Arrow::render(Shader& shader, const glm::mat4& model_matrix) const {
		shader.setMat4("model", model_matrix);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());
		shader.setFloat("objectAlpha", GetA());

		// Render Rod
		glBindVertexArray(rod_vao_);
		glDrawArrays(GL_TRIANGLES, 0, rod_vertex_count_);

		// Render Cone
		glBindVertexArray(cone_vao_);
		glDrawArrays(GL_TRIANGLES, 0, cone_vertex_count_);

		glBindVertexArray(0);
	}

	glm::mat4 Arrow::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model = model * glm::mat4_cast(GetRotation());
		model = glm::scale(model, GetScale());
		return model;
	}

	void Arrow::InitArrowMesh(Megabuffer* mb) const {
		if (rod_vao_ != 0 || rod_alloc_.valid)
			return;

		// Rod generation
		std::vector<float> rod_vertices;
		int                segments = 16;
		float              length = 1.0f - cone_height_;
		for (int i = 0; i < segments; ++i) {
			float angle1 = i * 2 * glm::pi<float>() / segments;
			float angle2 = (i + 1) * 2 * glm::pi<float>() / segments;

			glm::vec3 p1(rod_radius_ * cos(angle1), 0, rod_radius_ * sin(angle1));
			glm::vec3 p2(rod_radius_ * cos(angle2), 0, rod_radius_ * sin(angle2));
			glm::vec3 p3(rod_radius_ * cos(angle1), length, rod_radius_ * sin(angle1));
			glm::vec3 p4(rod_radius_ * cos(angle2), length, rod_radius_ * sin(angle2));

			// Side
			glm::vec3 n1 = glm::normalize(glm::vec3(p1.x, 0, p1.z));
			glm::vec3 n2 = glm::normalize(glm::vec3(p2.x, 0, p2.z));
			glm::vec3 n3 = glm::normalize(glm::vec3(p3.x, 0, p3.z));
			glm::vec3 n4 = glm::normalize(glm::vec3(p4.x, 0, p4.z));

			rod_vertices.insert(rod_vertices.end(), {p1.x, p1.y, p1.z, n1.x, n1.y, n1.z});
			rod_vertices.insert(rod_vertices.end(), {p2.x, p2.y, p2.z, n2.x, n2.y, n2.z});
			rod_vertices.insert(rod_vertices.end(), {p3.x, p3.y, p3.z, n3.x, n3.y, n3.z});

			rod_vertices.insert(rod_vertices.end(), {p2.x, p2.y, p2.z, n2.x, n2.y, n2.z});
			rod_vertices.insert(rod_vertices.end(), {p4.x, p4.y, p4.z, n4.x, n4.y, n4.z});
			rod_vertices.insert(rod_vertices.end(), {p3.x, p3.y, p3.z, n3.x, n3.y, n3.z});
		}
		rod_vertex_count_ = rod_vertices.size() / 6;

		// Cone generation
		std::vector<float> cone_vertices;
		glm::vec3          tip(0, 1.0, 0);
		for (int i = 0; i < segments; ++i) {
			float angle1 = i * 2 * glm::pi<float>() / segments;
			float angle2 = (i + 1) * 2 * glm::pi<float>() / segments;

			glm::vec3 p1(cone_radius_ * cos(angle1), length, cone_radius_ * sin(angle1));
			glm::vec3 p2(cone_radius_ * cos(angle2), length, cone_radius_ * sin(angle2));

			// Side
			glm::vec3 n1 = glm::normalize(glm::vec3(p1.x, cone_radius_, p1.z));
			glm::vec3 n2 = glm::normalize(glm::vec3(p2.x, cone_radius_, p2.z));
			glm::vec3 tip_normal = glm::normalize(glm::cross(p2 - p1, tip - p1));

			cone_vertices.insert(cone_vertices.end(), {p1.x, p1.y, p1.z, n1.x, n1.y, n1.z});
			cone_vertices.insert(cone_vertices.end(), {p2.x, p2.y, p2.z, n2.x, n2.y, n2.z});
			cone_vertices.insert(cone_vertices.end(), {tip.x, tip.y, tip.z, tip_normal.x, tip_normal.y, tip_normal.z});

			// Base
			glm::vec3 normal = glm::vec3(0, -1, 0);
			cone_vertices.insert(cone_vertices.end(), {p1.x, p1.y, p1.z, normal.x, normal.y, normal.z});
			cone_vertices.insert(cone_vertices.end(), {p2.x, p2.y, p2.z, normal.x, normal.y, normal.z});
			cone_vertices.insert(cone_vertices.end(), {0, length, 0, normal.x, normal.y, normal.z});
		}
		cone_vertex_count_ = cone_vertices.size() / 6;

		if (mb) {
			std::vector<Vertex> v_rod;
			for (size_t i = 0; i < rod_vertices.size(); i += 6) {
				Vertex v;
				v.Position = {rod_vertices[i], rod_vertices[i + 1], rod_vertices[i + 2]};
				v.Normal = {rod_vertices[i + 3], rod_vertices[i + 4], rod_vertices[i + 5]};
				v.TexCoords = {0, 0};
				v.Color = {1, 1, 1};
				v_rod.push_back(v);
			}
			rod_alloc_ = mb->AllocateStatic(v_rod.size(), 0);
			if (rod_alloc_.valid) {
				mb->Upload(rod_alloc_, v_rod.data(), v_rod.size());
				rod_vao_ = mb->GetVAO();
			}

			std::vector<Vertex> v_cone;
			for (size_t i = 0; i < cone_vertices.size(); i += 6) {
				Vertex v;
				v.Position = {cone_vertices[i], cone_vertices[i + 1], cone_vertices[i + 2]};
				v.Normal = {cone_vertices[i + 3], cone_vertices[i + 4], cone_vertices[i + 5]};
				v.TexCoords = {0, 0};
				v.Color = {1, 1, 1};
				v_cone.push_back(v);
			}
			cone_alloc_ = mb->AllocateStatic(v_cone.size(), 0);
			if (cone_alloc_.valid) {
				mb->Upload(cone_alloc_, v_cone.data(), v_cone.size());
				cone_vao_ = mb->GetVAO();
			}
		} else {
			std::vector<Vertex> v_rod;
			for (size_t i = 0; i < rod_vertices.size(); i += 6) {
				Vertex v;
				v.Position = {rod_vertices[i], rod_vertices[i + 1], rod_vertices[i + 2]};
				v.Normal = {rod_vertices[i + 3], rod_vertices[i + 4], rod_vertices[i + 5]};
				v.TexCoords = {0, 0};
				v.Color = {1, 1, 1};
				v_rod.push_back(v);
			}

			glGenVertexArrays(1, &rod_vao_);
			glBindVertexArray(rod_vao_);
			glGenBuffers(1, &rod_vbo_);
			glBindBuffer(GL_ARRAY_BUFFER, rod_vbo_);
			glBufferData(GL_ARRAY_BUFFER, v_rod.size() * sizeof(Vertex), v_rod.data(), GL_STATIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));
			glEnableVertexAttribArray(8);

			std::vector<Vertex> v_cone;
			for (size_t i = 0; i < cone_vertices.size(); i += 6) {
				Vertex v;
				v.Position = {cone_vertices[i], cone_vertices[i + 1], cone_vertices[i + 2]};
				v.Normal = {cone_vertices[i + 3], cone_vertices[i + 4], cone_vertices[i + 5]};
				v.TexCoords = {0, 0};
				v.Color = {1, 1, 1};
				v_cone.push_back(v);
			}

			glGenVertexArrays(1, &cone_vao_);
			glBindVertexArray(cone_vao_);
			glGenBuffers(1, &cone_vbo_);
			glBindBuffer(GL_ARRAY_BUFFER, cone_vbo_);
			glBufferData(GL_ARRAY_BUFFER, v_cone.size() * sizeof(Vertex), v_cone.data(), GL_STATIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));
			glEnableVertexAttribArray(8);

			glBindVertexArray(0);
		}
	}

	void Arrow::SetDirection(const glm::vec3& direction) {
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		glm::vec3 norm_direction = glm::normalize(direction);

		float dot = glm::dot(up, norm_direction);
		if (std::abs(dot + 1.0f) < 0.000001f) {
			// vector a and b are parallel but opposite direction
			SetRotation(glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));
			return;
		}
		if (std::abs(dot - 1.0f) < 0.000001f) {
			// vector a and b are parallel in same direction
			SetRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
			return;
		}

		glm::vec3 axis = glm::cross(up, norm_direction);
		float     angle = acos(dot);
		SetRotation(glm::angleAxis(angle, axis));
	}

	void Arrow::DestroyArrowMesh() {
		if (rod_vao_ != 0) {
			glDeleteVertexArrays(1, &rod_vao_);
			glDeleteBuffers(1, &rod_vbo_);
			rod_vao_ = 0;
		}
		if (cone_vao_ != 0) {
			glDeleteVertexArrays(1, &cone_vao_);
			glDeleteBuffers(1, &cone_vbo_);
			cone_vao_ = 0;
		}
	}

	void Arrow::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		glm::mat4 model_matrix = GetModelMatrix();
		glm::vec3 world_pos = glm::vec3(model_matrix[3]);

		// Arrow has two parts: Rod and Cone
		// For simplicity, we create two packets if needed, or one if they share state.
		// Since they use different VAOs, we need two packets.

		// Rod Packet
		{
			RenderPacket packet;
			packet.vao = rod_vao_;
			packet.vbo = rod_vbo_;
			if (rod_alloc_.valid) {
				packet.base_vertex = rod_alloc_.base_vertex;
			}
			packet.vertex_count = static_cast<unsigned int>(rod_vertex_count_);
			packet.draw_mode = GL_TRIANGLES;
			packet.shader_id = shader ? shader->ID : 0;
			packet.uniforms.model = model_matrix;
			packet.uniforms.color = glm::vec4(GetR(), GetG(), GetB(), GetA());
			packet.uniforms.use_pbr = UsePBR();
			packet.uniforms.roughness = GetRoughness();
			packet.uniforms.metallic = GetMetallic();
			packet.uniforms.ao = GetAO();
			packet.uniforms.is_colossal = IsColossal();

			RenderLayer layer = (GetA() < 0.99f) ? RenderLayer::Transparent : RenderLayer::Opaque;
			float       normalized_depth = context.CalculateNormalizedDepth(world_pos);
			packet.shader_handle = shader_handle;
			packet.material_handle = MaterialHandle(0);
			packet.sort_key = CalculateSortKey(
				layer,
				packet.shader_handle,
				packet.vao,
				packet.draw_mode,
				packet.index_count > 0,
				packet.material_handle,
				normalized_depth,
				false
			);
			out_packets.push_back(packet);
		}

		// Cone Packet
		{
			RenderPacket packet;
			packet.vao = cone_vao_;
			packet.vbo = cone_vbo_;
			if (cone_alloc_.valid) {
				packet.base_vertex = cone_alloc_.base_vertex;
			}
			packet.vertex_count = static_cast<unsigned int>(cone_vertex_count_);
			packet.draw_mode = GL_TRIANGLES;
			packet.shader_id = shader ? shader->ID : 0;
			packet.uniforms.model = model_matrix;
			packet.uniforms.color = glm::vec4(GetR(), GetG(), GetB(), GetA());
			packet.uniforms.use_pbr = UsePBR();
			packet.uniforms.roughness = GetRoughness();
			packet.uniforms.metallic = GetMetallic();
			packet.uniforms.ao = GetAO();
			packet.uniforms.is_colossal = IsColossal();

			packet.casts_shadows = CastsShadows();

			RenderLayer layer = (GetA() < 0.99f) ? RenderLayer::Transparent : RenderLayer::Opaque;
			float       normalized_depth = context.CalculateNormalizedDepth(world_pos);
			packet.shader_handle = shader_handle;
			packet.material_handle = MaterialHandle(0);
			packet.sort_key = CalculateSortKey(
				layer,
				packet.shader_handle,
				packet.vao,
				packet.draw_mode,
				packet.index_count > 0,
				packet.material_handle,
				normalized_depth,
				false
			);
			out_packets.push_back(packet);
		}
	}

} // namespace Boidsish
