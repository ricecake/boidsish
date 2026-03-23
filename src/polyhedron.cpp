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
	std::mutex                                     Polyhedron::s_mesh_mutex;

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
		Shape(id, x, y, z, r, g, b, a), type_(type), size_(size) {}

	Polyhedron::~Polyhedron() {}

	void Polyhedron::render() const {
		render(*shader, GetModelMatrix());
	}

	void Polyhedron::render(Shader& shader, const glm::mat4& model_matrix) const {
		EnsureMeshInitialized();
		std::lock_guard<std::mutex> lock(s_mesh_mutex);
		auto                        it = s_meshes.find(type_);
		if (it == s_meshes.end() || it->second.vao == 0)
			return;

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

		glBindVertexArray(it->second.vao);
		if (it->second.alloc.valid) {
			glDrawElementsBaseVertex(
				GL_TRIANGLES,
				it->second.index_count,
				GL_UNSIGNED_INT,
				(void*)(uintptr_t)(it->second.alloc.first_index * sizeof(unsigned int)),
				it->second.alloc.base_vertex
			);
		} else {
			glDrawElements(GL_TRIANGLES, it->second.index_count, GL_UNSIGNED_INT, 0);
		}
		glBindVertexArray(0);
	}

	glm::mat4 Polyhedron::GetModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
		model *= glm::mat4_cast(GetRotation());
		model = glm::scale(model, GetScale() * size_);
		return model;
	}

	void Polyhedron::GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const {
		EnsureMeshInitialized(context.megabuffer);
		std::lock_guard<std::mutex> lock(s_mesh_mutex);
		auto                        it = s_meshes.find(type_);
		if (it == s_meshes.end() || it->second.vao == 0)
			return;

		glm::mat4 model = GetModelMatrix();
		glm::vec3 world_pos = glm::vec3(model[3]);

		RenderPacket packet;
		packet.vao = it->second.vao;
		if (it->second.alloc.valid) {
			packet.base_vertex = it->second.alloc.base_vertex;
			packet.first_index = it->second.alloc.first_index;
		}
		packet.index_count = static_cast<unsigned int>(it->second.index_count);
		packet.draw_mode = GL_TRIANGLES;
		packet.index_type = GL_UNSIGNED_INT;
		packet.shader_id = shader ? shader->ID : 0;

		packet.uniforms.model = model;
		packet.uniforms.color = glm::vec4(GetR(), GetG(), GetB(), GetA());
		packet.uniforms.use_pbr = UsePBR();
		packet.uniforms.roughness = GetRoughness();
		packet.uniforms.metallic = GetMetallic();
		packet.uniforms.ao = GetAO();
		packet.uniforms.use_texture = false;
		packet.uniforms.is_colossal = IsColossal();

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
		RenderLayer layer = (GetA() < 0.99f) ? RenderLayer::Transparent : RenderLayer::Opaque;

		packet.shader_handle = shader_handle;
		packet.material_handle = MaterialHandle(0);

		// Platonic solids are convex, Kepler-Poinsot are not
		bool is_kepler_poinsot = (type_ == PolyhedronType::SmallStellatedDodecahedron ||
		                          type_ == PolyhedronType::GreatDodecahedron ||
		                          type_ == PolyhedronType::GreatStellatedDodecahedron ||
		                          type_ == PolyhedronType::GreatIcosahedron);
		packet.no_cull = is_kepler_poinsot || dissolve_enabled_;

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

		out_packets.push_back(packet);
	}

	bool Polyhedron::Intersects(const Ray& ray, float& t) const {
		return GetAABB().Intersects(ray, t);
	}

	AABB Polyhedron::GetAABB() const {
		EnsureMeshInitialized();
		std::lock_guard<std::mutex> lock(s_mesh_mutex);
		return s_meshes[type_].local_aabb.Transform(GetModelMatrix());
	}

	std::string Polyhedron::GetInstanceKey() const {
		return "Polyhedron:" + std::to_string(static_cast<int>(type_));
	}

	void Polyhedron::PrepareResources(Megabuffer* megabuffer) const {
		EnsureMeshInitialized(megabuffer);
	}

	void Polyhedron::EnsureMeshInitialized(Megabuffer* megabuffer) const {
		std::lock_guard<std::mutex> lock(s_mesh_mutex);
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
			unsigned  base = vertices.size();

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

		void AddQuad(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			const glm::vec3&           p0,
			const glm::vec3&           p1,
			const glm::vec3&           p2,
			const glm::vec3&           p3
		) {
			AddTriangle(vertices, indices, p0, p1, p2);
			AddTriangle(vertices, indices, p0, p2, p3);
		}

		void AddPentagon(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			const glm::vec3&           p0,
			const glm::vec3&           p1,
			const glm::vec3&           p2,
			const glm::vec3&           p3,
			const glm::vec3&           p4
		) {
			AddTriangle(vertices, indices, p0, p1, p2);
			AddTriangle(vertices, indices, p0, p2, p3);
			AddTriangle(vertices, indices, p0, p3, p4);
		}

		void AddPentagram(
			std::vector<Vertex>&       vertices,
			std::vector<unsigned int>& indices,
			const glm::vec3&           p0,
			const glm::vec3&           p1,
			const glm::vec3&           p2,
			const glm::vec3&           p3,
			const glm::vec3&           p4
		) {
			// A pentagram has 5 points: p0, p1, p2, p3, p4.
			// We can render this as 5 triangles from a center point to each edge,
			// or as 5 overlapping triangles to get the true star intersection look.
			// Let's use overlapping triangles for the star look.
			AddTriangle(vertices, indices, p0, p2, p4);
			AddTriangle(vertices, indices, p1, p3, p0);
			AddTriangle(vertices, indices, p2, p4, p1);
			AddTriangle(vertices, indices, p3, p0, p2);
			AddTriangle(vertices, indices, p4, p1, p3);
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

			AddTriangle(vertices, indices, p0, p2, p1);
			AddTriangle(vertices, indices, p0, p3, p2);
			AddTriangle(vertices, indices, p0, p1, p3);
			AddTriangle(vertices, indices, p1, p2, p3);
		} else if (type == PolyhedronType::Cube) {
			float     s = 1.0f;
			glm::vec3 pts[8] = {
				{-s, -s, s}, {s, -s, s}, {s, s, s}, {-s, s, s}, {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s}
			};
			AddQuad(vertices, indices, pts[0], pts[1], pts[2], pts[3]); // Front
			AddQuad(vertices, indices, pts[1], pts[5], pts[6], pts[2]); // Right
			AddQuad(vertices, indices, pts[5], pts[4], pts[7], pts[6]); // Back
			AddQuad(vertices, indices, pts[4], pts[0], pts[3], pts[7]); // Left
			AddQuad(vertices, indices, pts[3], pts[2], pts[6], pts[7]); // Top
			AddQuad(vertices, indices, pts[4], pts[5], pts[1], pts[0]); // Bottom
		} else if (type == PolyhedronType::Octahedron) {
			float     s = 1.0f;
			glm::vec3 pts[6] = {{s, 0, 0}, {-s, 0, 0}, {0, s, 0}, {0, -s, 0}, {0, 0, s}, {0, 0, -s}};
			AddTriangle(vertices, indices, pts[4], pts[0], pts[2]);
			AddTriangle(vertices, indices, pts[4], pts[2], pts[1]);
			AddTriangle(vertices, indices, pts[4], pts[1], pts[3]);
			AddTriangle(vertices, indices, pts[4], pts[3], pts[0]);
			AddTriangle(vertices, indices, pts[5], pts[2], pts[0]);
			AddTriangle(vertices, indices, pts[5], pts[1], pts[2]);
			AddTriangle(vertices, indices, pts[5], pts[3], pts[1]);
			AddTriangle(vertices, indices, pts[5], pts[0], pts[3]);
		} else if (type == PolyhedronType::Dodecahedron || type == PolyhedronType::GreatStellatedDodecahedron) {
			float                  phi = (1.0f + sqrt(5.0f)) / 2.0f;
			float                  inv_phi = 1.0f / phi;
			std::vector<glm::vec3> pts = {
				{1, 1, 1},       {1, 1, -1},      {1, -1, 1},      {1, -1, -1},
				{-1, 1, 1},      {-1, 1, -1},     {-1, -1, 1},     {-1, -1, -1},
				{0, inv_phi, phi}, {0, inv_phi, -phi}, {0, -inv_phi, phi}, {0, -inv_phi, -phi},
				{inv_phi, phi, 0}, {inv_phi, -phi, 0}, {-inv_phi, phi, 0}, {-inv_phi, -phi, 0},
				{phi, 0, inv_phi}, {phi, 0, -inv_phi}, {-phi, 0, inv_phi}, {-phi, 0, -inv_phi}
			};

			std::vector<std::vector<int>> faces = {
				{12, 1, 17, 16, 0},  {12, 0, 8, 4, 14},   {12, 14, 5, 9, 1},    {13, 2, 10, 6, 15},
				{13, 15, 7, 11, 3},  {13, 3, 17, 16, 2},  {8, 0, 16, 2, 10},    {8, 10, 6, 18, 4},
				{4, 18, 19, 5, 14},  {5, 19, 7, 11, 9},   {9, 11, 3, 17, 1},    {6, 18, 19, 7, 15}
			};

			if (type == PolyhedronType::Dodecahedron) {
				for (auto& f : faces) {
					AddPentagon(vertices, indices, pts[f[0]], pts[f[1]], pts[f[2]], pts[f[3]], pts[f[4]]);
				}
			} else {
				// Great Stellated Dodecahedron {5/2, 3}
				// Pentagram faces using vertices of dodecahedron.
				for (auto& f : faces) {
					AddPentagram(vertices, indices, pts[f[0]], pts[f[1]], pts[f[2]], pts[f[3]], pts[f[4]]);
				}
			}
		} else if (
			type == PolyhedronType::Icosahedron || type == PolyhedronType::SmallStellatedDodecahedron ||
			type == PolyhedronType::GreatDodecahedron || type == PolyhedronType::GreatIcosahedron
		) {
			float                  phi = (1.0f + sqrt(5.0f)) / 2.0f;
			std::vector<glm::vec3> pts = {
				{-1, phi, 0},  {1, phi, 0},  {-1, -phi, 0}, {1, -phi, 0},
				{0, -1, phi},  {0, 1, phi},  {0, -1, -phi}, {0, 1, -phi},
				{phi, 0, -1},  {phi, 0, 1},  {-phi, 0, -1}, {-phi, 0, 1}
			};

			std::vector<std::vector<int>> faces = {
				{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
				{1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
				{3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
				{4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
			};

			if (type == PolyhedronType::Icosahedron) {
				for (auto& f : faces) {
					AddTriangle(vertices, indices, pts[f[0]], pts[f[1]], pts[f[2]]);
				}
			} else if (type == PolyhedronType::SmallStellatedDodecahedron) {
				// SSD {5/2, 5}. 12 pentagrams.
				for (int i = 0; i < 12; ++i) {
					std::vector<int> neighbors;
					for (auto& f : faces) {
						if (f[0] == i) { neighbors.push_back(f[1]); neighbors.push_back(f[2]); }
						else if (f[1] == i) { neighbors.push_back(f[0]); neighbors.push_back(f[2]); }
						else if (f[2] == i) { neighbors.push_back(f[0]); neighbors.push_back(f[1]); }
					}
					std::vector<int> sorted;
					if(!neighbors.empty()){
						sorted.push_back(neighbors[0]);
						while(sorted.size() < 5){
							int last = sorted.back();
							for(size_t j = 0; j < neighbors.size(); ++j){
								int n = neighbors[j];
								bool already = false;
								for(int s : sorted) if(s == n) already = true;
								if(already) continue;
								for(auto& f : faces){
									bool has_i = (f[0] == i || f[1] == i || f[2] == i);
									bool has_last = (f[0] == last || f[1] == last || f[2] == last);
									bool has_n = (f[0] == n || f[1] == n || f[2] == n);
									if(has_i && has_last && has_n){
										sorted.push_back(n);
										goto next_n;
									}
								}
							}
							break;
							next_n:;
						}
					}
					if(sorted.size() == 5){
						AddPentagram(vertices, indices, pts[sorted[0]], pts[sorted[1]], pts[sorted[2]], pts[sorted[3]], pts[sorted[4]]);
					}
				}
			} else if (type == PolyhedronType::GreatDodecahedron) {
				for (int i = 0; i < 12; ++i) {
					std::vector<int> neighbors;
					for (auto& f : faces) {
						if (f[0] == i) { neighbors.push_back(f[1]); neighbors.push_back(f[2]); }
						else if (f[1] == i) { neighbors.push_back(f[0]); neighbors.push_back(f[2]); }
						else if (f[2] == i) { neighbors.push_back(f[0]); neighbors.push_back(f[1]); }
					}
					std::vector<int> sorted;
					if(!neighbors.empty()){
						sorted.push_back(neighbors[0]);
						while(sorted.size() < 5){
							int last = sorted.back();
							for(size_t j = 0; j < neighbors.size(); ++j){
								int n = neighbors[j];
								bool already = false;
								for(int s : sorted) if(s == n) already = true;
								if(already) continue;
								for(auto& f : faces){
									bool has_i = (f[0] == i || f[1] == i || f[2] == i);
									bool has_last = (f[0] == last || f[1] == last || f[2] == last);
									bool has_n = (f[0] == n || f[1] == n || f[2] == n);
									if(has_i && has_last && has_n){
										sorted.push_back(n);
										goto next_n2;
									}
								}
							}
							break;
							next_n2:;
						}
					}
					if(sorted.size() == 5){
						AddPentagon(vertices, indices, pts[sorted[0]], pts[sorted[1]], pts[sorted[2]], pts[sorted[3]], pts[sorted[4]]);
					}
				}
			} else if (type == PolyhedronType::GreatIcosahedron) {
				// Great Icosahedron {3, 5/2}. 20 triangles.
				// Vertices of icosahedron, but faces connect vertices that are "far" apart.
				// For each face (a, b, c) of icosahedron, the Great Icosahedron face can be
				// found by jumping over one vertex.
				// Actually, the faces of Great Icosahedron are the triangles whose edges are
				// the "long" diagonals of the pentagonal faces of the Great Dodecahedron.
				// A simpler way: for each vertex i, its 5 neighbors n1..n5.
				// The triangles are (n1, n3, n5), (n2, n4, n1), etc? No.
				// Correct approach for Great Icosahedron {3, 5/2}:
				// Vertices: same as Icosahedron.
				// Faces: 20 triangles. Each face is (pts[i], pts[j], pts[k]) such that they form
				// a large equilateral triangle.
				// From reference: the faces of Great Icosahedron are the same as the faces
				// of the icosahedron stellation that consists of 20 large triangles.
				// I will use a stellation logic: each triangle is formed by 3 vertices
				// whose distances are larger than the icosahedron edge.
				for(int i = 0; i < 12; ++i){
					for(int j = i+1; j < 12; ++j){
						for(int k = j+1; k < 12; ++k){
							float d1 = glm::distance(pts[i], pts[j]);
							float d2 = glm::distance(pts[j], pts[k]);
							float d3 = glm::distance(pts[k], pts[i]);
							// The side length of Great Icosahedron is 2.
							// For our icosahedron with coords like (1, phi, 0), the edge length is 2.
							// The side length of GI is larger.
							// Wait, the standard icosahedron with these coords has edge length 2.
							// The GI with same vertices has edge length 2*phi?
							if(std::abs(d1 - 2.0f*phi) < 0.1f && std::abs(d2 - 2.0f*phi) < 0.1f && std::abs(d3 - 2.0f*phi) < 0.1f){
								AddTriangle(vertices, indices, pts[i], pts[j], pts[k]);
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

		s_meshes[type].index_count = indices.size();

		if (megabuffer) {
			s_meshes[type].alloc = megabuffer->AllocateStatic(vertices.size(), indices.size());
			if (s_meshes[type].alloc.valid) {
				megabuffer->Upload(s_meshes[type].alloc, vertices.data(), vertices.size(), indices.data(), indices.size());
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
		std::lock_guard<std::mutex> lock(s_mesh_mutex);
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
