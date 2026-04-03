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
	std::atomic<int>        Shape::s_nextId{1};

	void Shape::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		MeshInfo mesh = GetMeshInfo(context.megabuffer);
		if (mesh.vao == 0)
			return;

		RenderPacket packet;
		PopulatePacket(packet, mesh, context);
		out_packets.push_back(std::move(packet));
	}

	void Shape::PopulatePacket(RenderPacket& packet, const MeshInfo& mesh, const RenderContext& context) const {
		glm::mat4 model = GetModelMatrix();
		glm::vec3 world_pos = glm::vec3(model[3]);

		packet.vao = mesh.vao;
		packet.vbo = mesh.vbo;
		packet.ebo = mesh.ebo;
		if (mesh.allocation.valid) {
			packet.base_vertex = mesh.allocation.base_vertex;
			packet.first_index = mesh.allocation.first_index;
		}
		packet.vertex_count = mesh.vertex_count;
		packet.index_count = mesh.index_count;
		packet.draw_mode = GL_TRIANGLES;
		packet.index_type = GL_UNSIGNED_INT;
		packet.shader_id = shader ? shader->ID : 0;

		packet.uniforms.model = model;
		packet.uniforms.color = glm::vec4(r_, g_, b_, a_);
		packet.uniforms.use_pbr = use_pbr_;
		packet.uniforms.roughness = roughness_;
		packet.uniforms.metallic = metallic_;
		packet.uniforms.ao = ao_;
		packet.uniforms.use_texture = false;
		packet.uniforms.is_colossal = is_colossal_;

		packet.uniforms.dissolve_enabled = dissolve_enabled_ ? 1 : 0;
		packet.uniforms.dissolve_plane_normal = dissolve_plane_normal_;
		packet.uniforms.dissolve_plane_dist = dissolve_plane_dist_;
		packet.uniforms.is_refractive = is_refractive_ ? 1 : 0;
		packet.uniforms.refractive_index = refractive_index_;

		AABB      worldAABB = GetAABB();
		glm::vec3 velocity = world_pos - GetLastPosition();
		if (glm::dot(velocity, velocity) > 0.001f) {
			worldAABB.min = glm::min(worldAABB.min, worldAABB.min - velocity);
			worldAABB.max = glm::max(worldAABB.max, worldAABB.max + velocity);
		}
		packet.uniforms.aabb_min_x = worldAABB.min.x;
		packet.uniforms.aabb_min_y = worldAABB.min.y;
		packet.uniforms.aabb_min_z = worldAABB.min.z;
		packet.uniforms.aabb_max_x = worldAABB.max.x;
		packet.uniforms.aabb_max_y = worldAABB.max.y;
		packet.uniforms.aabb_max_z = worldAABB.max.z;

		packet.casts_shadows = CastsShadows();
		RenderLayer layer = IsTransparent() ? RenderLayer::Transparent : RenderLayer::Opaque;

		packet.shader_handle = shader_handle;
		packet.material_handle = MaterialHandle(0);
		packet.no_cull = ShouldDisableCulling();

		float normalized_depth = context.CalculateNormalizedDepth(world_pos);
		packet.sort_key = CalculateSortKey(
			layer,
			packet.shader_handle,
			packet.vao,
			packet.draw_mode,
			packet.index_count > 0,
			packet.material_handle,
			normalized_depth,
			packet.no_cull
		);
	}

	void Shape::render() const {
		if (shader) {
			render(*shader, GetModelMatrix());
		}
	}

	void Shape::render(Shader& shader, const glm::mat4& model_matrix) const {
		MeshInfo mesh = GetMeshInfo();
		if (mesh.vao == 0)
			return;

		shader.use();
		shader.setMat4("model", model_matrix);
		shader.setVec3("objectColor", GetR(), GetG(), GetB());
		shader.setFloat("objectAlpha", GetA());
		shader.setBool("use_texture", false);

		shader.setBool("usePBR", UsePBR());
		if (UsePBR()) {
			shader.setFloat("roughness", GetRoughness());
			shader.setFloat("metallic", GetMetallic());
			shader.setFloat("ao", GetAO());
		}

		OnPreRender(shader);

		glBindVertexArray(mesh.vao);
		if (mesh.allocation.valid) {
			if (mesh.index_count > 0) {
				glDrawElementsBaseVertex(
					mesh.draw_mode,
					mesh.index_count,
					GL_UNSIGNED_INT,
					(void*)(uintptr_t)(mesh.allocation.first_index * sizeof(unsigned int)),
					mesh.allocation.base_vertex
				);
			} else {
				glDrawArrays(mesh.draw_mode, (GLint)mesh.allocation.base_vertex, (GLsizei)mesh.vertex_count);
			}
		} else {
			if (mesh.index_count > 0) {
				glDrawElements(mesh.draw_mode, mesh.index_count, GL_UNSIGNED_INT, 0);
			} else {
				glDrawArrays(mesh.draw_mode, 0, mesh.vertex_count);
			}
		}
		glBindVertexArray(0);
	}

	glm::mat4 Shape::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(x_, y_, z_));
		model *= glm::mat4_cast(rotation_);
		model = glm::scale(model, scale_ * size_ * GetSizeMultiplier());
		return model;
	}

	MeshInfo Shape::GetMeshInfo(Megabuffer* megabuffer) const {
		if (sphere_vao_ == 0) {
			const_cast<Shape*>(this)->InitSphereMesh(megabuffer);
		}
		MeshInfo info;
		info.vao = sphere_vao_;
		info.vbo = sphere_vbo_;
		info.ebo = sphere_ebo_;
		info.vertex_count = 0; // Not used for spheres when ebo is present
		info.index_count = (unsigned int)sphere_vertex_count_;
		info.allocation = sphere_alloc_;
		return info;
	}

	bool Shape::ShouldDisableCulling() const {
		return dissolve_enabled_;
	}

	float Shape::GetBoundingRadius() const {
		AABB aabb = local_aabb_;
		// Incorporate scaling
		glm::vec3 dims = (aabb.max - aabb.min) * scale_ * size_ * GetSizeMultiplier();
		return glm::length(dims) * 0.5f;
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

	void Shape::SetScaleToMaxDimension(float max_dim, int axis) {
		if (axis < 0 || axis > 2)
			return;

		AABB  aabb = GetAABB();
		float current_dim = aabb.max[axis] - aabb.min[axis];

		if (current_dim > 0.0001f) {
			float factor = max_dim / current_dim;
			SetScale(scale_ * factor);
		} else {
			// If current scale is too small, try with unit scale to find the base dimension
			glm::vec3 original_scale = scale_;
			scale_ = glm::vec3(1.0f);
			aabb = GetAABB();
			current_dim = aabb.max[axis] - aabb.min[axis];
			if (current_dim > 0.0001f) {
				float factor = max_dim / current_dim;
				SetScale(glm::vec3(factor));
			} else {
				scale_ = original_scale; // Restore if we still can't determine dimension
			}
		}
	}

	void Shape::SetScaleRelativeTo(const Shape& other, float ratio, int axis) {
		if (axis < 0 || axis > 2)
			return;

		AABB  other_aabb = other.GetAABB();
		float target_dim = (other_aabb.max[axis] - other_aabb.min[axis]) * ratio;
		SetScaleToMaxDimension(target_dim, axis);
	}

	void Shape::SetScaleToFitInside(const Shape& other) {
		AABB current = GetAABB();
		AABB target = other.GetAABB();

		glm::vec3 current_dims = current.max - current.min;
		glm::vec3 target_dims = target.max - target.min;

		// If current dimensions are zero, try with unit scale
		if (glm::length(current_dims) < 0.0001f) {
			glm::vec3 original_scale = scale_;
			scale_ = glm::vec3(1.0f);
			current = GetAABB();
			current_dims = current.max - current.min;
			if (glm::length(current_dims) < 0.0001f) {
				scale_ = original_scale;
				return;
			}
		}

		float factor = std::numeric_limits<float>::max();
		bool  found = false;

		for (int i = 0; i < 3; ++i) {
			if (current_dims[i] > 0.0001f) {
				factor = std::min(factor, target_dims[i] / current_dims[i]);
				found = true;
			}
		}

		if (found && factor != std::numeric_limits<float>::max()) {
			SetScale(scale_ * factor);
		}
	}

	bool Shape::Intersects(const Ray& ray, float& t) const {
		return GetAABB().Intersects(ray, t);
	}

	AABB Shape::GetAABB() const {
		return local_aabb_.Transform(GetModelMatrix());
	}

	// --- SdfShape ---

	SdfShape::SdfShape(SdfVolumeManager& manager, const SdfSource& source):
		Shape(0, source.position.x, source.position.y, source.position.z),
		manager_(manager),
		source_(source) {
		SetPosition(source.position.x, source.position.y, source.position.z);
		source_id_ = manager_.AddSource(source_);
		initial_radius_ = source_.radius;
		initial_density_ = source_.density;
		initial_position_ = source_.position;
	}

	SdfShape::~SdfShape() {
		manager_.RemoveSource(source_id_);
	}

	void SdfShape::Update(float delta_time) {
		age_ += delta_time;

		if (lifetime_ > 0.0f) {
			float t = std::min(age_ / lifetime_, 1.0f);
			source_.normalized_time = t;

			// Non-linear expansion: fast initial burst, slowing down (sqrt curve)
			float expansion = std::sqrt(t);
			source_.radius = initial_radius_ * (1.0f + expansion * 8.0f);

			// Density peaks early then fades — fireball is densest shortly after detonation
			float density_curve = std::sin(t * 3.14159f) * (1.0f - t * 0.5f);
			source_.density = initial_density_ * std::max(0.0f, density_curve);

			// Emission fades as the explosion cools
			source_.emission = initial_density_ * 3.0f * (1.0f - t) * (1.0f - t);

			// Upward drift: hot gas rises, creating mushroom shape
			glm::vec3 pos = initial_position_;
			pos.y += expansion * initial_radius_ * 3.0f;
			source_.position = pos;
		} else {
			source_.position = GetPosition();
		}

		manager_.UpdateSource(source_id_, source_);
	}
} // namespace Boidsish
