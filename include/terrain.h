#pragma once

#include <vector>

#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {
    struct TerrainParameters {
        float frequency;
        float amplitude;
        float threshold;
    };

	class Terrain: public Shape {
	public:
		Terrain(const std::vector<float>& vertexData, const std::vector<unsigned int>& indices);
		~Terrain();

		void render() const override;
        void SetTerrainParameters(const TerrainParameters& params) { params_ = params; }

		static std::shared_ptr<Shader> terrain_shader_;

	private:
		void setupMesh(const std::vector<float>& vertexData, const std::vector<unsigned int>& indices);

		unsigned int vao_, vbo_, ebo_;
		int          index_count_;
        TerrainParameters params_;
	};

} // namespace Boidsish
