#include "shape.h"

#include <cmath>
#include <vector>

#include "shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	// Initialize static members
	unsigned int            Shape::sphere_vao_ = 0;
	unsigned int            Shape::sphere_vbo_ = 0;
	unsigned int            Shape::sphere_ebo_ = 0;
	int                     Shape::sphere_vertex_count_ = 0;
	MegabufferAllocation    Shape::sphere_alloc_;
	std::shared_ptr<Shader> Shape::shader = nullptr;
	ShaderHandle            Shape::shader_handle = ShaderHandle(0);

	void Shape::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		if (sphere_vao_ == 0) {
			InitSphereMesh(context.megabuffer);
		}

		// Calculate model matrix once
		glm::mat4 model = GetModelMatrix();
		glm::vec3 world_pos = glm::vec3(model[3]);

		// Note: We remove CPU frustum culling here to allow packet reuse across multiple passes
		// (e.g., main camera, reflection, shadows) with different frustums.
		// GPU-side frustum culling in the vertex shader handles visibility for each pass.

		RenderPacket packet;
		packet.vao = sphere_vao_;
		if (sphere_alloc_.valid) {
			packet.base_vertex = sphere_alloc_.base_vertex;
			packet.first_index = sphere_alloc_.first_index;
		}
		packet.index_count = static_cast<unsigned int>(sphere_vertex_count_);
		packet.draw_mode = GL_TRIANGLES;
		packet.index_type = GL_UNSIGNED_INT;
		packet.shader_id = shader ? shader->ID : 0;

		packet.uniforms.model = model;
		packet.uniforms.color = glm::vec4(r_, g_, b_, a_);
		packet.uniforms.use_pbr = use_pbr_;
		packet.uniforms.roughness = roughness_;
		packet.uniforms.metallic = metallic_;
		packet.uniforms.ao = ao_;
		packet.uniforms.use_texture = false; // Default for base sphere shape
		packet.uniforms.is_colossal = is_colossal_;

		packet.uniforms.dissolve_enabled = dissolve_enabled_ ? 1 : 0;
		packet.uniforms.dissolve_plane_normal = dissolve_plane_normal_;
		packet.uniforms.dissolve_plane_dist = dissolve_plane_dist_;

		packet.casts_shadows = CastsShadows();

		// Default to Opaque layer unless alpha is less than 1.0
		RenderLayer layer = (a_ < 0.99f) ? RenderLayer::Transparent : RenderLayer::Opaque;

		// Handles would typically be managed by a higher-level system (e.g., AssetManager)
		packet.shader_handle = shader_handle;
		packet.material_handle = MaterialHandle(0);

		// Calculate depth for sorting
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

	void Shape::GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const {
		const int   latitude_segments = 16;
		const int   longitude_segments = 32;
		const float radius = 1.0f;

		for (int lat = 0; lat <= latitude_segments; ++lat) {
			for (int lon = 0; lon <= longitude_segments; ++lon) {
				float  theta = lat * glm::pi<float>() / latitude_segments;
				float  phi = lon * 2 * glm::pi<float>() / longitude_segments;
				float  x = radius * sin(theta) * cos(phi);
				float  y = radius * cos(theta);
				float  z = radius * sin(theta) * sin(phi);
				Vertex v;
				v.Position = glm::vec3(x, y, z);
				v.Normal = glm::vec3(x, y, z);
				v.TexCoords = glm::vec2((float)lon / longitude_segments, (float)lat / latitude_segments);
				vertices.push_back(v);
			}
		}

		for (int lat = 0; lat < latitude_segments; ++lat) {
			for (int lon = 0; lon < longitude_segments; ++lon) {
				int first = (lat * (longitude_segments + 1)) + lon;
				int second = first + longitude_segments + 1;
				// CCW winding
				indices.push_back(first);
				indices.push_back(first + 1);
				indices.push_back(second);

				indices.push_back(second);
				indices.push_back(first + 1);
				indices.push_back(second + 1);
			}
		}
	}

	void Shape::InitSphereMesh(Megabuffer* megabuffer) {
		if (sphere_vao_ != 0)
			return; // Already initialized

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;

		const int   latitude_segments = 16;
		const int   longitude_segments = 32;
		const float radius = 1.0f;

		for (int lat = 0; lat <= latitude_segments; ++lat) {
			for (int lon = 0; lon <= longitude_segments; ++lon) {
				float  theta = lat * glm::pi<float>() / latitude_segments;
				float  phi = lon * 2 * glm::pi<float>() / longitude_segments;
				float  x = radius * sin(theta) * cos(phi);
				float  y = radius * cos(theta);
				float  z = radius * sin(theta) * sin(phi);
				Vertex v;
				v.Position = glm::vec3(x, y, z);
				v.Normal = glm::vec3(x, y, z);
				v.TexCoords = glm::vec2((float)lon / longitude_segments, (float)lat / latitude_segments);
				vertices.push_back(v);
			}
		}

		for (int lat = 0; lat < latitude_segments; ++lat) {
			for (int lon = 0; lon < longitude_segments; ++lon) {
				int first = (lat * (longitude_segments + 1)) + lon;
				int second = first + longitude_segments + 1;
				// CCW winding
				indices.push_back(first);
				indices.push_back(first + 1);
				indices.push_back(second);

				indices.push_back(second);
				indices.push_back(first + 1);
				indices.push_back(second + 1);
			}
		}
		sphere_vertex_count_ = indices.size();

		if (megabuffer) {
			sphere_alloc_ = megabuffer->AllocateStatic(vertices.size(), indices.size());
			if (sphere_alloc_.valid) {
				megabuffer->Upload(sphere_alloc_, vertices.data(), vertices.size(), indices.data(), indices.size());
				sphere_vao_ = megabuffer->GetVAO();
			}
		} else {
			glGenVertexArrays(1, &sphere_vao_);
			glBindVertexArray(sphere_vao_);

			glGenBuffers(1, &sphere_vbo_);
			glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

			glGenBuffers(1, &sphere_ebo_);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo_);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

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

	void Shape::DestroySphereMesh() {
		if (sphere_vao_ != 0) {
			glDeleteVertexArrays(1, &sphere_vao_);
			glDeleteBuffers(1, &sphere_vbo_);
			if (sphere_ebo_ != 0) {
				glDeleteBuffers(1, &sphere_ebo_);
			}
			sphere_vao_ = 0;
			sphere_vbo_ = 0;
			sphere_ebo_ = 0;
			sphere_vertex_count_ = 0;
		}
	}

	void Shape::RenderSphere(
		const glm::vec3& position,
		const glm::vec3& color,
		const glm::vec3& scale,
		const glm::quat& rotation
	) {
		if (sphere_vao_ == 0)
			return;

		shader->use();
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, position);
		model = model * glm::mat4_cast(rotation);
		model = glm::scale(model, scale);
		shader->setMat4("model", model);
		shader->setVec3("objectColor", color.r, color.g, color.b);
		shader->setFloat("objectAlpha", 1.0f);

		glBindVertexArray(sphere_vao_);
		if (sphere_alloc_.valid) {
			glDrawElementsBaseVertex(
				GL_TRIANGLES,
				sphere_vertex_count_,
				GL_UNSIGNED_INT,
				(void*)(uintptr_t)(sphere_alloc_.first_index * sizeof(unsigned int)),
				sphere_alloc_.base_vertex
			);
		} else {
			glDrawElements(GL_TRIANGLES, sphere_vertex_count_, GL_UNSIGNED_INT, 0);
		}
		glBindVertexArray(0);
	}

	void Shape::LookAt(const glm::vec3& target, const glm::vec3& up) {
		rotation_ = glm::quat_cast(glm::inverse(glm::lookAt(glm::vec3(x_, y_, z_), target, up)));
		MarkDirty();
	}

	bool Shape::Intersects(const Ray& ray, float& t) const {
		return GetAABB().Intersects(ray, t);
	}

	AABB Shape::GetAABB() const {
		return local_aabb_.Transform(GetModelMatrix());
	}
} // namespace Boidsish
