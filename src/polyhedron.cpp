#include "polyhedron.h"

#include <cmath>
#include <map>
#include <mutex>
#include <vector>

#include "shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	std::map<PolyhedronType, Polyhedron::MeshData> Polyhedron::s_meshes;
	std::recursive_mutex                           Polyhedron::s_mesh_mutex;

	Polyhedron::Polyhedron(
		PolyhedronType type,
		int            id,
		float          x,
		float          y,
		float          z,
		float          size,
		float          r,
		float          g,
		float          b,
		float          a
	):
		Shape(id, x, y, z, size, r, g, b, a), type_(type) {}

	Polyhedron::~Polyhedron() {}

	AABB Polyhedron::GetAABB() const {
		EnsureMeshInitialized();
		std::lock_guard<std::recursive_mutex> lock(s_mesh_mutex);
		return s_meshes.at(type_).local_aabb.Transform(GetModelMatrix());
	}

	std::string Polyhedron::GetInstanceKey() const {
		return "Polyhedron:" + std::to_string(static_cast<int>(type_));
	}

	void Polyhedron::PrepareResources(Megabuffer* megabuffer) const { EnsureMeshInitialized(megabuffer); }

	MeshInfo Polyhedron::GetMeshInfo(Megabuffer* megabuffer) const {
		EnsureMeshInitialized(megabuffer);
		std::lock_guard<std::recursive_mutex> lock(s_mesh_mutex);
		const auto&                           data = s_meshes.at(type_);
		MeshInfo                              info;
		info.vao = data.vao;
		info.vbo = data.vbo;
		info.ebo = data.ebo;
		info.vertex_count = 0;
		info.index_count = static_cast<unsigned int>(data.index_count);
		info.allocation = data.alloc;
		return info;
	}

	bool Polyhedron::ShouldDisableCulling() const {
		// Platonic solids are convex, Kepler-Poinsot are not
		bool is_kepler_poinsot =
			(type_ == PolyhedronType::SmallStellatedDodecahedron || type_ == PolyhedronType::GreatDodecahedron ||
		     type_ == PolyhedronType::GreatStellatedDodecahedron || type_ == PolyhedronType::GreatIcosahedron);
		return is_kepler_poinsot || dissolve_enabled_;
	}

	void Polyhedron::EnsureMeshInitialized(Megabuffer* megabuffer) const {
		std::lock_guard<std::recursive_mutex> lock(s_mesh_mutex);
		if (s_meshes.count(type_) == 0 || (s_meshes[type_].vao == 0 && !s_meshes[type_].alloc.valid)) {
			InitPolyhedronMesh(type_, megabuffer);
		}
		// Sync local_aabb_ with the static one
		const_cast<Polyhedron*>(this)->local_aabb_ = s_meshes[type_].local_aabb;
	}

	namespace {
		void AddTriangle(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			const glm::vec3&           p0,
			const glm::vec3&           p1,
			const glm::vec3&           p2
		) {
			glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
			unsigned  base = static_cast<unsigned int>(vertices.size());

			Vertex v;
			v.Normal = normal;
			v.Color = glm::vec3(1.0f);
			v.TexCoords = glm::vec2(0.5f);

			v.Position = p0;
			vertices.push_back(v);
			v.Position = p1;
			vertices.push_back(v);
			v.Position = p2;
			vertices.push_back(v);

			indices.push_back(base);
			indices.push_back(base + 1);
			indices.push_back(base + 2);
		}

		/**
		 * @brief Adds a triangle and ensures its winding is such that the normal points away from the center.
		 */
		void AddTriangleOutward(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			const glm::vec3&           p0,
			const glm::vec3&           p1,
			const glm::vec3&           p2,
			const glm::vec3&           center = glm::vec3(0.0f)
		) {
			glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
			glm::vec3 to_face = (p0 + p1 + p2) / 3.0f - center;
			if (glm::dot(normal, to_face) < 0.0f) {
				AddTriangle(vertices, indices, p0, p2, p1);
			} else {
				AddTriangle(vertices, indices, p0, p1, p2);
			}
		}

		void AddPentagonOutward(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			const glm::vec3&           p0,
			const glm::vec3&           p1,
			const glm::vec3&           p2,
			const glm::vec3&           p3,
			const glm::vec3&           p4,
			const glm::vec3&           center = glm::vec3(0.0f)
		) {
			AddTriangleOutward(vertices, indices, p0, p1, p2, center);
			AddTriangleOutward(vertices, indices, p0, p2, p3, center);
			AddTriangleOutward(vertices, indices, p0, p3, p4, center);
		}

		void AddPentagramOutward(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			const glm::vec3&           p0,
			const glm::vec3&           p1,
			const glm::vec3&           p2,
			const glm::vec3&           p3,
			const glm::vec3&           p4,
			const glm::vec3&           center = glm::vec3(0.0f)
		) {
			// A pentagram has 5 points. We render it as 5 overlapping triangles for the star look.
			// This represents the self-intersecting 'star' topology.
			AddTriangleOutward(vertices, indices, p0, p2, p4, center);
			AddTriangleOutward(vertices, indices, p1, p3, p0, center);
			AddTriangleOutward(vertices, indices, p2, p4, p1, center);
			AddTriangleOutward(vertices, indices, p3, p0, p2, center);
			AddTriangleOutward(vertices, indices, p4, p1, p3, center);
		}
	} // namespace

	void Polyhedron::InitPolyhedronMesh(PolyhedronType type, Megabuffer* megabuffer) {
		// Assume mutex is locked by caller EnsureMeshInitialized

		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;

		if (type == PolyhedronType::Tetrahedron) {
			float     s = 1.0f;
			glm::vec3 p0(s, s, s);
			glm::vec3 p1(-s, -s, s);
			glm::vec3 p2(-s, s, -s);
			glm::vec3 p3(s, -s, -s);

			AddTriangleOutward(vertices, indices, p0, p1, p2);
			AddTriangleOutward(vertices, indices, p0, p2, p3);
			AddTriangleOutward(vertices, indices, p0, p3, p1);
			AddTriangleOutward(vertices, indices, p1, p3, p2);
		} else if (type == PolyhedronType::Cube) {
			float     s = 1.0f;
			glm::vec3 pts[8] =
				{{-s, -s, s}, {s, -s, s}, {s, s, s}, {-s, s, s}, {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s}};
			AddTriangleOutward(vertices, indices, pts[0], pts[1], pts[2]);
			AddTriangleOutward(vertices, indices, pts[0], pts[2], pts[3]);
			AddTriangleOutward(vertices, indices, pts[1], pts[5], pts[6]);
			AddTriangleOutward(vertices, indices, pts[1], pts[6], pts[2]);
			AddTriangleOutward(vertices, indices, pts[5], pts[4], pts[7]);
			AddTriangleOutward(vertices, indices, pts[5], pts[7], pts[6]);
			AddTriangleOutward(vertices, indices, pts[4], pts[0], pts[3]);
			AddTriangleOutward(vertices, indices, pts[4], pts[3], pts[7]);
			AddTriangleOutward(vertices, indices, pts[3], pts[2], pts[6]);
			AddTriangleOutward(vertices, indices, pts[3], pts[6], pts[7]);
			AddTriangleOutward(vertices, indices, pts[4], pts[5], pts[1]);
			AddTriangleOutward(vertices, indices, pts[4], pts[1], pts[0]);
		} else if (type == PolyhedronType::Octahedron) {
			float     s = 1.0f;
			glm::vec3 pts[6] = {{s, 0, 0}, {-s, 0, 0}, {0, s, 0}, {0, -s, 0}, {0, 0, s}, {0, 0, -s}};
			AddTriangleOutward(vertices, indices, pts[4], pts[0], pts[2]);
			AddTriangleOutward(vertices, indices, pts[4], pts[2], pts[1]);
			AddTriangleOutward(vertices, indices, pts[4], pts[1], pts[3]);
			AddTriangleOutward(vertices, indices, pts[4], pts[3], pts[0]);
			AddTriangleOutward(vertices, indices, pts[5], pts[2], pts[0]);
			AddTriangleOutward(vertices, indices, pts[5], pts[1], pts[2]);
			AddTriangleOutward(vertices, indices, pts[5], pts[3], pts[1]);
			AddTriangleOutward(vertices, indices, pts[5], pts[0], pts[3]);
		} else if (type == PolyhedronType::Dodecahedron || type == PolyhedronType::GreatStellatedDodecahedron) {
			float                  phi = (1.0f + sqrt(5.0f)) / 2.0f;
			float                  inv_phi = 1.0f / phi;
			std::vector<glm::vec3> pts = {
				{1, 1, 1},           {1, 1, -1},          {1, -1, 1},         {1, -1, -1},        {-1, 1, 1},
				{-1, 1, -1},         {-1, -1, 1},         {-1, -1, -1},       {0, inv_phi, phi},  {0, inv_phi, -phi},
				{0, -inv_phi, phi},  {0, -inv_phi, -phi}, {inv_phi, phi, 0},  {inv_phi, -phi, 0}, {-inv_phi, phi, 0},
				{-inv_phi, -phi, 0}, {phi, 0, inv_phi},   {phi, 0, -inv_phi}, {-phi, 0, inv_phi}, {-phi, 0, -inv_phi}
			};

			std::vector<std::vector<int>> faces = {
				{12, 1, 17, 16, 0},
				{12, 0, 8, 4, 14},
				{12, 14, 5, 9, 1},
				{13, 2, 10, 6, 15},
				{13, 15, 7, 11, 3},
				{13, 3, 17, 16, 2},
				{8, 0, 16, 2, 10},
				{8, 10, 6, 18, 4},
				{4, 18, 19, 5, 14},
				{5, 19, 7, 11, 9},
				{9, 11, 3, 17, 1},
				{6, 18, 19, 7, 15}
			};

			if (type == PolyhedronType::Dodecahedron) {
				for (auto& f : faces) {
					AddPentagonOutward(vertices, indices, pts[f[0]], pts[f[1]], pts[f[2]], pts[f[3]], pts[f[4]]);
				}
			} else {
				// Great Stellated Dodecahedron {5/2, 3}
				for (auto& f : faces) {
					AddPentagramOutward(vertices, indices, pts[f[0]], pts[f[1]], pts[f[2]], pts[f[3]], pts[f[4]]);
				}
			}
		} else if (
			type == PolyhedronType::Icosahedron || type == PolyhedronType::SmallStellatedDodecahedron ||
			type == PolyhedronType::GreatDodecahedron || type == PolyhedronType::GreatIcosahedron
		) {
			float                  phi = (1.0f + sqrt(5.0f)) / 2.0f;
			std::vector<glm::vec3> pts = {
				{-1, phi, 0},
				{1, phi, 0},
				{-1, -phi, 0},
				{1, -phi, 0},
				{0, -1, phi},
				{0, 1, phi},
				{0, -1, -phi},
				{0, 1, -phi},
				{phi, 0, -1},
				{phi, 0, 1},
				{-phi, 0, -1},
				{-phi, 0, 1}
			};

			std::vector<std::vector<int>> faces = {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
			                                       {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
			                                       {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
			                                       {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};

			if (type == PolyhedronType::Icosahedron) {
				for (auto& f : faces) {
					AddTriangleOutward(vertices, indices, pts[f[0]], pts[f[1]], pts[f[2]]);
				}
			} else if (type == PolyhedronType::SmallStellatedDodecahedron) {
				// SSD {5/2, 5}. 12 pentagrams.
				for (int i = 0; i < 12; ++i) {
					std::vector<int> neighbors;
					for (auto& f : faces) {
						if (f[0] == i) {
							neighbors.push_back(f[1]);
							neighbors.push_back(f[2]);
						} else if (f[1] == i) {
							neighbors.push_back(f[0]);
							neighbors.push_back(f[2]);
						} else if (f[2] == i) {
							neighbors.push_back(f[0]);
							neighbors.push_back(f[1]);
						}
					}
					std::vector<int> sorted;
					if (!neighbors.empty()) {
						sorted.push_back(neighbors[0]);
						while (sorted.size() < 5) {
							int last = sorted.back();
							for (size_t j = 0; j < neighbors.size(); ++j) {
								int  n = neighbors[j];
								bool already = false;
								for (int s : sorted)
									if (s == n)
										already = true;
								if (already)
									continue;
								for (auto& f : faces) {
									bool has_i = (f[0] == i || f[1] == i || f[2] == i);
									bool has_last = (f[0] == last || f[1] == last || f[2] == last);
									bool has_n = (f[0] == n || f[1] == n || f[2] == n);
									if (has_i && has_last && has_n) {
										sorted.push_back(n);
										goto next_n;
									}
								}
							}
							break;
						next_n:;
						}
					}
					if (sorted.size() == 5) {
						AddPentagramOutward(
							vertices,
							indices,
							pts[sorted[0]],
							pts[sorted[1]],
							pts[sorted[2]],
							pts[sorted[3]],
							pts[sorted[4]]
						);
					}
				}
			} else if (type == PolyhedronType::GreatDodecahedron) {
				for (int i = 0; i < 12; ++i) {
					std::vector<int> neighbors;
					for (auto& f : faces) {
						if (f[0] == i) {
							neighbors.push_back(f[1]);
							neighbors.push_back(f[2]);
						} else if (f[1] == i) {
							neighbors.push_back(f[0]);
							neighbors.push_back(f[2]);
						} else if (f[2] == i) {
							neighbors.push_back(f[0]);
							neighbors.push_back(f[1]);
						}
					}
					std::vector<int> sorted;
					if (!neighbors.empty()) {
						sorted.push_back(neighbors[0]);
						while (sorted.size() < 5) {
							int last = sorted.back();
							for (size_t j = 0; j < neighbors.size(); ++j) {
								int  n = neighbors[j];
								bool already = false;
								for (int s : sorted)
									if (s == n)
										already = true;
								if (already)
									continue;
								for (auto& f : faces) {
									bool has_i = (f[0] == i || f[1] == i || f[2] == i);
									bool has_last = (f[0] == last || f[1] == last || f[2] == last);
									bool has_n = (f[0] == n || f[1] == n || f[2] == n);
									if (has_i && has_last && has_n) {
										sorted.push_back(n);
										goto next_n2;
									}
								}
							}
							break;
						next_n2:;
						}
					}
					if (sorted.size() == 5) {
						AddPentagonOutward(
							vertices,
							indices,
							pts[sorted[0]],
							pts[sorted[1]],
							pts[sorted[2]],
							pts[sorted[3]],
							pts[sorted[4]]
						);
					}
				}
			} else if (type == PolyhedronType::GreatIcosahedron) {
				// Great Icosahedron {3, 5/2}. 20 triangles.
				// Search for equilateral triangles with edge length 2*phi (large diagonals of icosahedron)
				for (int i = 0; i < 12; ++i) {
					for (int j = i + 1; j < 12; ++j) {
						for (int k = j + 1; k < 12; ++k) {
							float d1 = glm::distance(pts[i], pts[j]);
							float d2 = glm::distance(pts[j], pts[k]);
							float d3 = glm::distance(pts[k], pts[i]);
							if (std::abs(d1 - 2.0f * phi) < 0.1f && std::abs(d2 - 2.0f * phi) < 0.1f &&
							    std::abs(d3 - 2.0f * phi) < 0.1f) {
								AddTriangleOutward(vertices, indices, pts[i], pts[j], pts[k]);
							}
						}
					}
				}
			}
		}

		// Calculate AABB
		if (!vertices.empty()) {
			glm::vec3 min = vertices[0].Position;
			glm::vec3 max = vertices[0].Position;
			for (const auto& v : vertices) {
				min = glm::min(min, v.Position);
				max = glm::max(max, v.Position);
			}
			s_meshes[type].local_aabb = AABB(min, max);
		} else {
			s_meshes[type].local_aabb = AABB(glm::vec3(-1.0f), glm::vec3(1.0f));
		}

		s_meshes[type].index_count = static_cast<int>(indices.size());

		if (megabuffer) {
			s_meshes[type].alloc = megabuffer->AllocateStatic(
				static_cast<uint32_t>(vertices.size()),
				static_cast<uint32_t>(indices.size())
			);
			if (s_meshes[type].alloc.valid) {
				megabuffer->Upload(
					s_meshes[type].alloc,
					vertices.data(),
					static_cast<uint32_t>(vertices.size()),
					indices.data(),
					static_cast<uint32_t>(indices.size())
				);
				s_meshes[type].vao = megabuffer->GetVAO();
			}
		} else {
			glGenVertexArrays(1, &s_meshes[type].vao);
			glBindVertexArray(s_meshes[type].vao);

			glGenBuffers(1, &s_meshes[type].vbo);
			glBindBuffer(GL_ARRAY_BUFFER, s_meshes[type].vbo);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

			glGenBuffers(1, &s_meshes[type].ebo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_meshes[type].ebo);
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

	void Polyhedron::DestroyPolyhedronMeshes() {
		std::lock_guard<std::recursive_mutex> lock(s_mesh_mutex);
		for (auto& pair : s_meshes) {
			if (pair.second.vao != 0 && !pair.second.alloc.valid) {
				glDeleteVertexArrays(1, &pair.second.vao);
				glDeleteBuffers(1, &pair.second.vbo);
				glDeleteBuffers(1, &pair.second.ebo);
			}
		}
		s_meshes.clear();
	}

} // namespace Boidsish
