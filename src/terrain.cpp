#include "terrain.h"

#include "graphics.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <shader.h>

namespace Boidsish {

	std::shared_ptr<Shader> Terrain::terrain_shader_ = nullptr;

	Terrain::Terrain(
		const std::vector<unsigned int>& indices,
		const std::vector<glm::vec3>&    vertices,
		const std::vector<glm::vec3>&    normals,
		const PatchProxy&                proxy
	):
		indices_(indices),
		vertices(vertices),
		normals(normals),
		proxy(proxy),
		vao_(0),
		vbo_(0),
		ebo_(0),
		index_count_(indices.size()) {
		// Constructor now only initializes member variables
		// setupMesh() must be called explicitly to upload to GPU
	}

	Terrain::~Terrain() {
		glDeleteVertexArrays(1, &vao_);
		glDeleteBuffers(1, &vbo_);
		glDeleteBuffers(1, &ebo_);
	}

	bool rayTriangleIntersect(
		const glm::vec3& orig,
		const glm::vec3& dir,
		const glm::vec3& v0,
		const glm::vec3& v1,
		const glm::vec3& v2,
		float&           t,
		float&           u,
		float&           v
	) {
		glm::vec3 v0v1 = v1 - v0;
		glm::vec3 v0v2 = v2 - v0;
		glm::vec3 pvec = glm::cross(dir, v0v2);
		float     det = glm::dot(v0v1, pvec);

		if (std::abs(det) < 1e-8)
			return false;
		float invDet = 1 / det;

		glm::vec3 tvec = orig - v0;
		u = glm::dot(tvec, pvec) * invDet;
		if (u < 0 || u > 1)
			return false;

		glm::vec3 qvec = glm::cross(tvec, v0v1);
		v = glm::dot(dir, qvec) * invDet;
		if (v < 0 || u + v > 1)
			return false;

		t = glm::dot(v0v2, qvec) * invDet;

		return t > 0;
	}

	void Terrain::setupMesh() {
		// Build Octree
		Octree<size_t>::AABB chunk_aabb;
		chunk_aabb.min = glm::vec3(std::numeric_limits<float>::max());
		chunk_aabb.max = glm::vec3(std::numeric_limits<float>::lowest());

		for (const auto& v : vertices) {
			chunk_aabb.min = glm::min(chunk_aabb.min, v);
			chunk_aabb.max = glm::max(chunk_aabb.max, v);
		}

		octree_ = std::make_unique<Octree<size_t>>(chunk_aabb);

		for (size_t i = 0; i < indices_.size(); i += 4) {
			unsigned int i0 = indices_[i];
			unsigned int i1 = indices_[i + 1];
			unsigned int i2 = indices_[i + 2];
			unsigned int i3 = indices_[i + 3];

			glm::vec3 v0 = vertices[i0];
			glm::vec3 v1 = vertices[i1];
			glm::vec3 v2 = vertices[i2];
			glm::vec3 v3 = vertices[i3];

			// Tri 1
			Octree<size_t>::AABB tri1_aabb;
			tri1_aabb.min = glm::min(glm::min(v0, v1), v2);
			tri1_aabb.max = glm::max(glm::max(v0, v1), v2);
			octree_->insert({tri1_aabb, i});

			// Tri 2
			Octree<size_t>::AABB tri2_aabb;
			tri2_aabb.min = glm::min(glm::min(v0, v2), v3);
			tri2_aabb.max = glm::max(glm::max(v0, v2), v3);
			octree_->insert({tri2_aabb, i + 1}); // Use i+1 to distinguish Tri 2
		}

		// Generate interleaved vertex data for GPU upload
		vertex_data_.reserve(vertices.size() * 8);
		for (size_t i = 0; i < vertices.size(); ++i) {
			vertex_data_.push_back(vertices[i].x);
			vertex_data_.push_back(vertices[i].y);
			vertex_data_.push_back(vertices[i].z);
			vertex_data_.push_back(normals[i].x);
			vertex_data_.push_back(normals[i].y);
			vertex_data_.push_back(normals[i].z);
			// Dummy texture coordinates
			vertex_data_.push_back(0.0f);
			vertex_data_.push_back(0.0f);
		}

		glGenVertexArrays(1, &vao_);
		glGenBuffers(1, &vbo_);
		glGenBuffers(1, &ebo_);

		glBindVertexArray(vao_);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertex_data_.size() * sizeof(float), &vertex_data_[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(unsigned int), &indices_[0], GL_STATIC_DRAW);

		// Position attribute
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// Normal attribute
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// Texture coordinate attribute
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);

		// Clear interleaved data after uploading to GPU, but keep vertices and normals for physics
		vertex_data_.clear();
		// indices_.clear(); // Keep indices for Octree raycasting
	}

	void Terrain::render() const {
		terrain_shader_->use();
		terrain_shader_->setMat4("model", GetModelMatrix());

		glBindVertexArray(vao_);
		glPatchParameteri(GL_PATCH_VERTICES, 4);
		glDrawElements(GL_PATCHES, index_count_, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	void Terrain::render(Shader& shader, const glm::mat4& model_matrix) const {
		// Terrain is not meant to be cloned, so this is a no-op
	}

	glm::mat4 Terrain::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		return model;
	}

	bool Terrain::raycast(const Octree<size_t>::Ray& ray, float& out_dist, glm::vec3& out_normal) const {
		if (!octree_)
			return false;

		// The ray passed to this function is in world space.
		// Transform it to local space for the Octree query.
		glm::vec3 local_origin = ray.origin - glm::vec3(GetX(), GetY(), GetZ());
		glm::vec3 local_dir = ray.direction; // Assuming no rotation/scale for terrain chunks
		Octree<size_t>::Ray local_ray(local_origin, local_dir);

		std::vector<size_t> candidate_tri_indices;
		octree_->raycast(local_ray, candidate_tri_indices);

		bool  hit = false;
		float closest_t = std::numeric_limits<float>::max();

		for (size_t tri_idx_info : candidate_tri_indices) {
			// tri_idx_info is either i or i+1 from indices_ loop
			// We need to figure out which triangle it is.
			// If even (roughly), it's Tri 1, else Tri 2? No, I used i and i+1.
			// Let's recover:
			size_t base_i = (tri_idx_info / 4) * 4;
			bool   is_tri2 = (tri_idx_info % 4) != 0;

			unsigned int i0, i1, i2;
			if (!is_tri2) {
				i0 = indices_[base_i];
				i1 = indices_[base_i + 1];
				i2 = indices_[base_i + 2];
			} else {
				i0 = indices_[base_i];
				i1 = indices_[base_i + 2];
				i2 = indices_[base_i + 3];
			}

			float t, u, v;
			if (rayTriangleIntersect(local_origin, local_dir, vertices[i0], vertices[i1], vertices[i2], t, u, v)) {
				if (t < closest_t) {
					closest_t = t;
					out_dist = t;
					// Interpolate normal or just use face normal
					glm::vec3 n0 = normals[i0];
					glm::vec3 n1 = normals[i1];
					glm::vec3 n2 = normals[i2];
					out_normal = glm::normalize(n0 * (1.0f - u - v) + n1 * u + n2 * v);
					hit = true;
				}
			}
		}

		return hit;
	}

} // namespace Boidsish
