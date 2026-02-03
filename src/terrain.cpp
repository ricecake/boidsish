#include "terrain.h"

namespace Boidsish {

	std::shared_ptr<Shader> Terrain::terrain_shader_ = nullptr;

	Terrain::Terrain(
		const std::vector<unsigned int>& indices,
		const std::vector<glm::vec3>&    vertices,
		const std::vector<glm::vec3>&    normals,
		const PatchProxy&                proxy
	):
		indices_(indices), vertices(vertices), normals(normals), proxy(proxy), index_count_(indices.size()) {}

	Terrain::~Terrain() = default;

	std::vector<float> Terrain::GetInterleavedVertexData() const {
		std::vector<float> data;
		data.reserve(vertices.size() * 8);
		for (size_t i = 0; i < vertices.size(); ++i) {
			data.push_back(vertices[i].x);
			data.push_back(vertices[i].y);
			data.push_back(vertices[i].z);
			data.push_back(normals[i].x);
			data.push_back(normals[i].y);
			data.push_back(normals[i].z);
			// Dummy texture coordinates
			data.push_back(0.0f);
			data.push_back(0.0f);
		}
		return data;
	}

} // namespace Boidsish
