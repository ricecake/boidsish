#include "delaunay_blob.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "logger.h"
#include "shader.h"

namespace Boidsish {

	// === Face comparison operators ===

	bool DelaunayBlob::Face::operator==(const Face& other) const {
		// Faces are equal if they have the same vertices (order-independent)
		std::array<int, 3> a = vertices;
		std::array<int, 3> b = other.vertices;
		std::sort(a.begin(), a.end());
		std::sort(b.begin(), b.end());
		return a == b;
	}

	bool DelaunayBlob::Face::operator<(const Face& other) const {
		std::array<int, 3> a = vertices;
		std::array<int, 3> b = other.vertices;
		std::sort(a.begin(), a.end());
		std::sort(b.begin(), b.end());
		return a < b;
	}

	// Hash for Face (for use in unordered containers)
	struct FaceHash {
		size_t operator()(const DelaunayBlob::Face& f) const {
			std::array<int, 3> sorted = f.vertices;
			std::sort(sorted.begin(), sorted.end());
			size_t h = 0;
			for (int v : sorted) {
				h ^= std::hash<int>()(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
			}
			return h;
		}
	};

	// === Constructor / Destructor ===

	DelaunayBlob::DelaunayBlob(int id): Shape(id), id_(id) {
		// GL buffers initialized lazily on first render
	}

	DelaunayBlob::~DelaunayBlob() {
		CleanupBuffers();
	}

	DelaunayBlob::DelaunayBlob(DelaunayBlob&& other) noexcept: Shape(std::move(other)) {
		points_ = std::move(other.points_);
		next_point_id_ = other.next_point_id_;
		id_ = other.id_;
		render_mode_ = other.render_mode_;
		alpha_ = other.alpha_;
		wireframe_color_ = other.wireframe_color_;
		smooth_normals_ = other.smooth_normals_;
		auto_retetrahedralize_ = other.auto_retetrahedralize_;
		mesh_dirty_ = other.mesh_dirty_;
		tetrahedra_ = std::move(other.tetrahedra_);
		surface_faces_ = std::move(other.surface_faces_);

		vao_ = other.vao_;
		wire_vao_ = other.wire_vao_;
		vbo_ = other.vbo_;
		ebo_ = other.ebo_;
		wire_ebo_ = other.wire_ebo_;
		index_count_ = other.index_count_;
		wire_index_count_ = other.wire_index_count_;
		buffers_initialized_ = other.buffers_initialized_;

		other.vao_ = 0;
		other.wire_vao_ = 0;
		other.vbo_ = 0;
		other.ebo_ = 0;
		other.wire_ebo_ = 0;
		other.buffers_initialized_ = false;
	}

	DelaunayBlob& DelaunayBlob::operator=(DelaunayBlob&& other) noexcept {
		if (this != &other) {
			CleanupBuffers();

			Shape::operator=(std::move(other));
			points_ = std::move(other.points_);
			next_point_id_ = other.next_point_id_;
			id_ = other.id_;
			render_mode_ = other.render_mode_;
			alpha_ = other.alpha_;
			wireframe_color_ = other.wireframe_color_;
			smooth_normals_ = other.smooth_normals_;
			auto_retetrahedralize_ = other.auto_retetrahedralize_;
			mesh_dirty_ = other.mesh_dirty_;
			tetrahedra_ = std::move(other.tetrahedra_);
			surface_faces_ = std::move(other.surface_faces_);

			vao_ = other.vao_;
			wire_vao_ = other.wire_vao_;
			vbo_ = other.vbo_;
			ebo_ = other.ebo_;
			wire_ebo_ = other.wire_ebo_;
			index_count_ = other.index_count_;
			wire_index_count_ = other.wire_index_count_;
			buffers_initialized_ = other.buffers_initialized_;

			other.vao_ = 0;
			other.wire_vao_ = 0;
			other.vbo_ = 0;
			other.ebo_ = 0;
			other.wire_ebo_ = 0;
			other.buffers_initialized_ = false;
		}
		return *this;
	}

	// === Point Management ===

	int DelaunayBlob::AddPoint(const glm::vec3& position) {
		int id = next_point_id_++;
		points_[id] = ControlPoint{id, position, glm::vec3(0.0f), glm::vec4(GetR(), GetG(), GetB(), alpha_)};

		if (auto_retetrahedralize_) {
			MarkDirty();
		}

		return id;
	}

	bool DelaunayBlob::AddPointWithId(int point_id, const glm::vec3& position) {
		if (points_.count(point_id) > 0) {
			return false;
		}

		points_[point_id] =
			ControlPoint{point_id, position, glm::vec3(0.0f), glm::vec4(GetR(), GetG(), GetB(), alpha_)};
		next_point_id_ = std::max(next_point_id_, point_id + 1);

		if (auto_retetrahedralize_) {
			MarkDirty();
		}

		return true;
	}

	void DelaunayBlob::RemovePoint(int point_id) {
		if (points_.erase(point_id) > 0 && auto_retetrahedralize_) {
			MarkDirty();
		}
	}

	void DelaunayBlob::SetPointPosition(int point_id, const glm::vec3& position) {
		auto it = points_.find(point_id);
		if (it != points_.end()) {
			it->second.position = position;
			if (auto_retetrahedralize_) {
				MarkDirty();
			}
		}
	}

	void DelaunayBlob::SetPointState(int point_id, const glm::vec3& position, const glm::vec3& velocity) {
		auto it = points_.find(point_id);
		if (it != points_.end()) {
			it->second.position = position;
			it->second.velocity = velocity;
			if (auto_retetrahedralize_) {
				MarkDirty();
			}
		}
	}

	void DelaunayBlob::SetPointColor(int point_id, const glm::vec4& color) {
		auto it = points_.find(point_id);
		if (it != points_.end()) {
			it->second.color = color;
			MarkDirty();
		}
	}

	std::optional<glm::vec3> DelaunayBlob::GetPointPosition(int point_id) const {
		auto it = points_.find(point_id);
		if (it != points_.end()) {
			return it->second.position;
		}
		return std::nullopt;
	}

	std::vector<int> DelaunayBlob::GetPointIds() const {
		std::vector<int> ids;
		ids.reserve(points_.size());
		for (const auto& [id, _] : points_) {
			ids.push_back(id);
		}
		return ids;
	}

	// === Bulk Operations ===

	std::vector<int> DelaunayBlob::AddPoints(const std::vector<glm::vec3>& positions) {
		std::vector<int> ids;
		ids.reserve(positions.size());

		bool was_auto = auto_retetrahedralize_;
		auto_retetrahedralize_ = false;

		for (const auto& pos : positions) {
			ids.push_back(AddPoint(pos));
		}

		auto_retetrahedralize_ = was_auto;
		if (auto_retetrahedralize_) {
			MarkDirty();
		}

		return ids;
	}

	void DelaunayBlob::SetPointPositions(const std::map<int, glm::vec3>& positions) {
		bool was_auto = auto_retetrahedralize_;
		auto_retetrahedralize_ = false;

		for (const auto& [id, pos] : positions) {
			SetPointPosition(id, pos);
		}

		auto_retetrahedralize_ = was_auto;
		if (auto_retetrahedralize_) {
			MarkDirty();
		}
	}

	void DelaunayBlob::Clear() {
		points_.clear();
		tetrahedra_.clear();
		surface_faces_.clear();
		MarkDirty();
	}

	// === Tetrahedralization ===

	void DelaunayBlob::Retetrahedralize() {
		ComputeDelaunay3D();
		ExtractSurfaceFaces();
		MarkDirty();
	}

	// === 3D Delaunay Algorithm ===

	std::pair<glm::vec3, float> DelaunayBlob::ComputeCircumsphere(
		const glm::vec3& a,
		const glm::vec3& b,
		const glm::vec3& c,
		const glm::vec3& d
	) const {
		// Compute circumsphere using determinant method
		// Reference: https://mathworld.wolfram.com/Circumsphere.html

		glm::vec3 ba = b - a;
		glm::vec3 ca = c - a;
		glm::vec3 da = d - a;

		float len_ba = glm::dot(ba, ba);
		float len_ca = glm::dot(ca, ca);
		float len_da = glm::dot(da, da);

		glm::vec3 cross_cd = glm::cross(ca, da);
		glm::vec3 cross_db = glm::cross(da, ba);
		glm::vec3 cross_bc = glm::cross(ba, ca);

		float denom = 2.0f * glm::dot(ba, cross_cd);

		if (std::abs(denom) < 1e-10f) {
			// Degenerate tetrahedron (coplanar points)
			glm::vec3 center = (a + b + c + d) * 0.25f;
			float     max_dist = std::max(
                {glm::distance(center, a), glm::distance(center, b), glm::distance(center, c), glm::distance(center, d)}
            );
			return {center, max_dist * max_dist * 1e6f}; // Large radius for degenerate case
		}

		glm::vec3 offset = (len_ba * cross_cd + len_ca * cross_db + len_da * cross_bc) / denom;
		glm::vec3 center = a + offset;
		float     radius_sq = glm::dot(offset, offset);

		return {center, radius_sq};
	}

	bool DelaunayBlob::InCircumsphere(const glm::vec3& p, const Tetrahedron& tet) const {
		float dist_sq = glm::dot(p - tet.circumcenter, p - tet.circumcenter);
		return dist_sq < tet.circumradius_sq * (1.0f + 1e-6f); // Small epsilon for numerical stability
	}

	std::array<glm::vec3, 4> DelaunayBlob::ComputeSuperTetrahedron() const {
		// Find bounding box of all points
		glm::vec3 min_pt(std::numeric_limits<float>::max());
		glm::vec3 max_pt(std::numeric_limits<float>::lowest());

		for (const auto& [_, cp] : points_) {
			min_pt = glm::min(min_pt, cp.position);
			max_pt = glm::max(max_pt, cp.position);
		}

		// Expand bounds
		glm::vec3 delta = max_pt - min_pt;
		float     dmax = std::max({delta.x, delta.y, delta.z}) * 3.0f;
		glm::vec3 mid = (min_pt + max_pt) * 0.5f;

		// Create super-tetrahedron vertices that contain all points with margin
		// Using a regular tetrahedron scaled and centered
		float s = dmax * 2.0f;

		return {
			{mid + glm::vec3(s, s, s),
		     mid + glm::vec3(s, -s, -s),
		     mid + glm::vec3(-s, s, -s),
		     mid + glm::vec3(-s, -s, s)}
		};
	}

	DelaunayBlob::Face DelaunayBlob::MakeFace(int v0, int v1, int v2) const {
		Face f;
		f.vertices = {v0, v1, v2};
		// Sort for canonical representation
		std::sort(f.vertices.begin(), f.vertices.end());
		return f;
	}

	void DelaunayBlob::ComputeDelaunay3D() const {
		tetrahedra_.clear();

		if (points_.size() < 4) {
			return; // Need at least 4 points for a tetrahedron
		}

		// Build ordered list of point IDs and positions
		std::vector<int>       point_ids;
		std::vector<glm::vec3> positions;
		std::map<int, size_t>  id_to_index;

		for (const auto& [id, cp] : points_) {
			id_to_index[id] = point_ids.size();
			point_ids.push_back(id);
			positions.push_back(cp.position);
		}

		// Create super-tetrahedron with special negative IDs
		auto      super = ComputeSuperTetrahedron();
		const int SUPER_0 = -1, SUPER_1 = -2, SUPER_2 = -3, SUPER_3 = -4;

		// Helper to get position by ID
		auto get_pos = [&](int id) -> glm::vec3 {
			if (id == SUPER_0)
				return super[0];
			if (id == SUPER_1)
				return super[1];
			if (id == SUPER_2)
				return super[2];
			if (id == SUPER_3)
				return super[3];
			return positions[id_to_index[id]];
		};

		// Working tetrahedra list
		struct WorkTet {
			std::array<int, 4> v;
			glm::vec3          center;
			float              radius_sq;
		};

		std::vector<WorkTet> work_tets;

		// Initialize with super-tetrahedron
		{
			auto [center, rsq] = ComputeCircumsphere(super[0], super[1], super[2], super[3]);
			work_tets.push_back({{SUPER_0, SUPER_1, SUPER_2, SUPER_3}, center, rsq});
		}

		// Insert each point using Bowyer-Watson
		for (size_t i = 0; i < point_ids.size(); ++i) {
			int       new_id = point_ids[i];
			glm::vec3 new_pos = positions[i];

			// Find tetrahedra whose circumsphere contains the new point
			std::vector<WorkTet> bad_tets;
			std::vector<WorkTet> good_tets;

			for (const auto& tet : work_tets) {
				float dist_sq = glm::dot(new_pos - tet.center, new_pos - tet.center);
				if (dist_sq < tet.radius_sq * (1.0f + 1e-6f)) {
					bad_tets.push_back(tet);
				} else {
					good_tets.push_back(tet);
				}
			}

			// Collect boundary faces of the cavity (faces that appear exactly once)
			std::map<std::array<int, 3>, int> face_count;

			auto add_face = [&](int a, int b, int c) {
				std::array<int, 3> face = {a, b, c};
				std::sort(face.begin(), face.end());
				face_count[face]++;
			};

			for (const auto& tet : bad_tets) {
				// Each tetrahedron has 4 faces
				add_face(tet.v[0], tet.v[1], tet.v[2]);
				add_face(tet.v[0], tet.v[1], tet.v[3]);
				add_face(tet.v[0], tet.v[2], tet.v[3]);
				add_face(tet.v[1], tet.v[2], tet.v[3]);
			}

			// Boundary faces appear exactly once
			std::vector<std::array<int, 3>> boundary_faces;
			for (const auto& [face, count] : face_count) {
				if (count == 1) {
					boundary_faces.push_back(face);
				}
			}

			// Create new tetrahedra connecting boundary faces to the new point
			work_tets = std::move(good_tets);

			for (const auto& face : boundary_faces) {
				glm::vec3 p0 = get_pos(face[0]);
				glm::vec3 p1 = get_pos(face[1]);
				glm::vec3 p2 = get_pos(face[2]);

				auto [center, rsq] = ComputeCircumsphere(p0, p1, p2, new_pos);
				work_tets.push_back({{face[0], face[1], face[2], new_id}, center, rsq});
			}
		}

		// Remove tetrahedra that include super-tetrahedron vertices
		for (const auto& tet : work_tets) {
			bool has_super = false;
			for (int v : tet.v) {
				if (v < 0) {
					has_super = true;
					break;
				}
			}

			if (!has_super) {
				Tetrahedron out_tet;
				out_tet.vertices = tet.v;
				out_tet.circumcenter = tet.center;
				out_tet.circumradius_sq = tet.radius_sq;
				tetrahedra_.push_back(out_tet);
			}
		}
	}

	glm::vec3 DelaunayBlob::ComputeFaceNormal(
		const glm::vec3& p0,
		const glm::vec3& p1,
		const glm::vec3& p2,
		const glm::vec3& opposite
	) const {
		glm::vec3 v1 = p1 - p0;
		glm::vec3 v2 = p2 - p0;
		glm::vec3 normal = glm::normalize(glm::cross(v1, v2));

		// Ensure normal points away from the opposite vertex (outward from tetrahedron)
		glm::vec3 to_opposite = opposite - p0;
		if (glm::dot(normal, to_opposite) > 0) {
			normal = -normal;
		}

		return normal;
	}

	void DelaunayBlob::ExtractSurfaceFaces() const {
		surface_faces_.clear();

		if (tetrahedra_.empty()) {
			return;
		}

		// Count face occurrences - boundary faces appear exactly once
		std::map<std::array<int, 3>, std::vector<std::pair<size_t, int>>> face_to_tet;

		for (size_t ti = 0; ti < tetrahedra_.size(); ++ti) {
			const auto& tet = tetrahedra_[ti];

			// 4 faces per tetrahedron, with opposite vertex index
			std::array<std::tuple<int, int, int, int>, 4> faces = {{
				{tet.vertices[1], tet.vertices[2], tet.vertices[3], 0}, // opposite to v0
				{tet.vertices[0], tet.vertices[2], tet.vertices[3], 1}, // opposite to v1
				{tet.vertices[0], tet.vertices[1], tet.vertices[3], 2}, // opposite to v2
				{tet.vertices[0], tet.vertices[1], tet.vertices[2], 3}  // opposite to v3
			}};

			for (const auto& [a, b, c, opp_idx] : faces) {
				std::array<int, 3> key = {a, b, c};
				std::sort(key.begin(), key.end());
				face_to_tet[key].push_back({ti, opp_idx});
			}
		}

		// Extract boundary faces (those appearing exactly once)
		for (const auto& [key, tet_list] : face_to_tet) {
			if (tet_list.size() == 1) {
				size_t      ti = tet_list[0].first;
				int         opp_idx = tet_list[0].second;
				const auto& tet = tetrahedra_[ti];

				// Get the original (unsorted) vertex order from the tetrahedron
				std::array<int, 3> original_verts;
				switch (opp_idx) {
				case 0:
					original_verts = {tet.vertices[1], tet.vertices[2], tet.vertices[3]};
					break;
				case 1:
					original_verts = {tet.vertices[0], tet.vertices[2], tet.vertices[3]};
					break;
				case 2:
					original_verts = {tet.vertices[0], tet.vertices[1], tet.vertices[3]};
					break;
				case 3:
					original_verts = {tet.vertices[0], tet.vertices[1], tet.vertices[2]};
					break;
				}

				// Get positions
				glm::vec3 p0 = points_.at(original_verts[0]).position;
				glm::vec3 p1 = points_.at(original_verts[1]).position;
				glm::vec3 p2 = points_.at(original_verts[2]).position;
				glm::vec3 opposite = points_.at(tet.vertices[opp_idx]).position;

				// Compute face normal and check if it points outward (away from opposite vertex)
				glm::vec3 v1 = p1 - p0;
				glm::vec3 v2 = p2 - p0;
				glm::vec3 normal = glm::normalize(glm::cross(v1, v2));
				glm::vec3 to_opposite = opposite - p0;

				Face face;
				// If normal points toward the opposite vertex, we need to flip the winding
				if (glm::dot(normal, to_opposite) > 0) {
					normal = -normal;
					// Swap v1 and v2 to flip winding order
					face.vertices = {original_verts[0], original_verts[2], original_verts[1]};
				} else {
					face.vertices = original_verts;
				}
				face.normal = normal;
				face.centroid = (p0 + p1 + p2) / 3.0f;

				surface_faces_.push_back(face);
			}
		}
	}

	// === OpenGL Buffer Management ===

	void DelaunayBlob::InitializeBuffers() const {
		if (buffers_initialized_)
			return;

		glGenVertexArrays(1, &vao_);
		glGenVertexArrays(1, &wire_vao_);
		glGenBuffers(1, &vbo_);
		glGenBuffers(1, &ebo_);
		glGenBuffers(1, &wire_ebo_);

		auto setup_vao = [&](GLuint vao) {
			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo_);

			// Position attribute
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

			// Normal attribute
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

			// Color attribute
			glEnableVertexAttribArray(8);
			glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
		};

		setup_vao(vao_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);

		setup_vao(wire_vao_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire_ebo_);

		glBindVertexArray(0);

		buffers_initialized_ = true;
	}

	void DelaunayBlob::CleanupBuffers() {
		if (vao_)
			glDeleteVertexArrays(1, &vao_);
		if (wire_vao_)
			glDeleteVertexArrays(1, &wire_vao_);
		if (vbo_)
			glDeleteBuffers(1, &vbo_);
		if (ebo_)
			glDeleteBuffers(1, &ebo_);
		if (wire_ebo_)
			glDeleteBuffers(1, &wire_ebo_);

		vao_ = wire_vao_ = vbo_ = ebo_ = wire_ebo_ = 0;
		buffers_initialized_ = false;
	}

	void DelaunayBlob::UpdateMeshBuffers() const {
		if (!buffers_initialized_) {
			InitializeBuffers();
		}

		if (mesh_dirty_) {
			ComputeDelaunay3D();
			ExtractSurfaceFaces();
		}

		if (surface_faces_.empty()) {
			index_count_ = 0;
			wire_index_count_ = 0;
			mesh_dirty_ = false;
			return;
		}

		std::vector<Vertex> vertices;
		std::vector<GLuint> indices;
		std::vector<GLuint> wire_indices;

		if (smooth_normals_) {
			// Average normals at shared vertices
			std::unordered_map<int, glm::vec3> vertex_normal_sum;
			std::unordered_map<int, int>       vertex_face_count;

			for (const auto& face : surface_faces_) {
				for (int vid : face.vertices) {
					vertex_normal_sum[vid] += face.normal;
					vertex_face_count[vid]++;
				}
			}

			// Create vertex buffer with averaged normals
			std::unordered_map<int, GLuint> point_to_vertex;

			for (const auto& [id, cp] : points_) {
				Vertex v;
				v.position = cp.position;
				if (vertex_face_count[id] > 0) {
					v.normal = glm::normalize(vertex_normal_sum[id] / static_cast<float>(vertex_face_count[id]));
				} else {
					v.normal = glm::vec3(0, 1, 0);
				}
				v.color = cp.color;

				point_to_vertex[id] = static_cast<GLuint>(vertices.size());
				vertices.push_back(v);
			}

			// Create indices
			for (const auto& face : surface_faces_) {
				indices.push_back(point_to_vertex[face.vertices[0]]);
				indices.push_back(point_to_vertex[face.vertices[1]]);
				indices.push_back(point_to_vertex[face.vertices[2]]);

				// Wireframe edges
				wire_indices.push_back(point_to_vertex[face.vertices[0]]);
				wire_indices.push_back(point_to_vertex[face.vertices[1]]);
				wire_indices.push_back(point_to_vertex[face.vertices[1]]);
				wire_indices.push_back(point_to_vertex[face.vertices[2]]);
				wire_indices.push_back(point_to_vertex[face.vertices[2]]);
				wire_indices.push_back(point_to_vertex[face.vertices[0]]);
			}
		} else {
			// Flat shading - duplicate vertices per face
			for (const auto& face : surface_faces_) {
				GLuint base = static_cast<GLuint>(vertices.size());

				const auto& p0 = points_.at(face.vertices[0]);
				const auto& p1 = points_.at(face.vertices[1]);
				const auto& p2 = points_.at(face.vertices[2]);

				vertices.push_back({p0.position, face.normal, p0.color});
				vertices.push_back({p1.position, face.normal, p1.color});
				vertices.push_back({p2.position, face.normal, p2.color});

				indices.push_back(base);
				indices.push_back(base + 1);
				indices.push_back(base + 2);

				wire_indices.push_back(base);
				wire_indices.push_back(base + 1);
				wire_indices.push_back(base + 1);
				wire_indices.push_back(base + 2);
				wire_indices.push_back(base + 2);
				wire_indices.push_back(base);
			}
		}

		// Upload to GPU
		glBindVertexArray(vao_);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_DYNAMIC_DRAW);

		index_count_ = indices.size();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire_ebo_);
		glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			wire_indices.size() * sizeof(GLuint),
			wire_indices.data(),
			GL_DYNAMIC_DRAW
		);
		wire_index_count_ = wire_indices.size();

		glBindVertexArray(0);

		mesh_dirty_ = false;
	}

	// === Rendering ===

	void DelaunayBlob::render() const {
		if (shader && shader->isValid()) {
			render(*shader, GetModelMatrix());
		}
	}

	void DelaunayBlob::render(Shader& active_shader, const glm::mat4& model_matrix) const {
		if (points_.size() < 4)
			return; // Need 4 points for tetrahedralization

		UpdateMeshBuffers();

		if (index_count_ == 0)
			return;

		active_shader.use();

		active_shader.setMat4("model", model_matrix);
		active_shader.setVec3("objectColor", GetR(), GetG(), GetB());
		active_shader.setFloat("objectAlpha", alpha_);
		active_shader.setInt("useVertexColor", 1);

		glBindVertexArray(vao_);

		switch (render_mode_) {
		case RenderMode::Solid:
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count_), GL_UNSIGNED_INT, 0);
			break;

		case RenderMode::Wireframe:
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire_ebo_);
			active_shader.setVec3("objectColor", wireframe_color_.r, wireframe_color_.g, wireframe_color_.b);
			glDrawElements(GL_LINES, static_cast<GLsizei>(wire_index_count_), GL_UNSIGNED_INT, 0);
			break;

		case RenderMode::SolidWithWire:
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count_), GL_UNSIGNED_INT, 0);

			glEnable(GL_POLYGON_OFFSET_LINE);
			glPolygonOffset(-1.0f, -1.0f);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire_ebo_);
			active_shader.setVec3("objectColor", wireframe_color_.r, wireframe_color_.g, wireframe_color_.b);
			active_shader.setInt("useVertexColor", 0);
			glDrawElements(GL_LINES, static_cast<GLsizei>(wire_index_count_), GL_UNSIGNED_INT, 0);
			glDisable(GL_POLYGON_OFFSET_LINE);
			break;

		case RenderMode::Transparent:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count_), GL_UNSIGNED_INT, 0);
			glDisable(GL_BLEND);
			break;
		}

		glBindVertexArray(0);
	}

	glm::mat4 DelaunayBlob::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model = model * glm::mat4_cast(rotation_);
		model = glm::scale(model, scale_);
		return model;
	}

	glm::vec3 DelaunayBlob::GetCentroid() const {
		if (points_.empty()) {
			return glm::vec3(GetX(), GetY(), GetZ());
		}

		glm::vec3 sum(0.0f);
		for (const auto& [_, cp] : points_) {
			sum += cp.position;
		}
		return sum / static_cast<float>(points_.size());
	}

	float DelaunayBlob::GetBoundingRadius() const {
		if (points_.empty())
			return 0.0f;

		glm::vec3 centroid = GetCentroid();
		float     max_dist_sq = 0.0f;

		for (const auto& [_, cp] : points_) {
			float dist_sq = glm::dot(cp.position - centroid, cp.position - centroid);
			max_dist_sq = std::max(max_dist_sq, dist_sq);
		}

		return std::sqrt(max_dist_sq);
	}

} // namespace Boidsish

namespace Boidsish {
	void DelaunayBlob::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		if (points_.empty())
			return;

		if (index_count_ == 0)
			return;

		glm::mat4 model_matrix = GetModelMatrix();
		glm::vec3 world_pos = GetCentroid();
		float     normalized_depth = context.CalculateNormalizedDepth(world_pos);

		auto create_packet = [&](RenderMode mode) {
			RenderPacket packet;
			packet.vao = vao_;
			packet.vbo = vbo_;
			packet.shader_id = shader ? shader->ID : 0;
			packet.shader_handle = shader_handle;
			packet.material_handle = MaterialHandle(0);
			packet.uniforms.model = model_matrix;
			packet.uniforms.use_pbr = UsePBR();
			packet.uniforms.roughness = GetRoughness();
			packet.uniforms.metallic = GetMetallic();
			packet.uniforms.ao = GetAO();
			packet.uniforms.use_texture = 0;
			packet.uniforms.use_vertex_color = 1;
			packet.uniforms.is_instanced = IsInstanced();
			packet.uniforms.is_colossal = IsColossal();
			packet.casts_shadows = CastsShadows();

			if (mode == RenderMode::Wireframe) {
				packet.vao = wire_vao_;
				packet.ebo = wire_ebo_;
				packet.index_count = static_cast<unsigned int>(wire_index_count_);
				packet.draw_mode = GL_LINES;
				packet.index_type = GL_UNSIGNED_INT;
				packet.uniforms.color = glm::vec4(wireframe_color_.r, wireframe_color_.g, wireframe_color_.b, 1.0f);
				packet.uniforms.use_vertex_color = 0; // Wireframe usually uses constant color
				packet.sort_key = CalculateSortKey(
					RenderLayer::Overlay,
					packet.shader_handle,
					packet.material_handle,
					normalized_depth
				);
			} else {
				packet.ebo = ebo_;
				packet.index_count = static_cast<unsigned int>(index_count_);
				packet.draw_mode = GL_TRIANGLES;
				packet.index_type = GL_UNSIGNED_INT;
				packet.uniforms.color = glm::vec4(GetR(), GetG(), GetB(), alpha_);

				RenderLayer layer = (alpha_ < 0.99f || mode == RenderMode::Transparent) ? RenderLayer::Transparent
																						: RenderLayer::Opaque;
				packet.sort_key =
					CalculateSortKey(layer, packet.shader_handle, packet.material_handle, normalized_depth);
			}
			return packet;
		};

		if (render_mode_ == RenderMode::SolidWithWire) {
			out_packets.push_back(create_packet(RenderMode::Solid));
			out_packets.push_back(create_packet(RenderMode::Wireframe));
		} else {
			out_packets.push_back(create_packet(render_mode_));
		}
	}
} // namespace Boidsish
