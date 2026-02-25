#pragma once

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <stack>

#include "model.h"

namespace Boidsish {

	class ProceduralGenerator {
	public:
		static std::shared_ptr<Model> GenerateRock(unsigned int seed);
		static std::shared_ptr<Model> GenerateGrass(unsigned int seed);
		static std::shared_ptr<Model> GenerateFlower(unsigned int seed);
		static std::shared_ptr<Model> GenerateTree(unsigned int seed);

	private:
		struct TurtleState {
			glm::vec3 position;
			glm::quat orientation;
			float     thickness;
		};

		static std::shared_ptr<ModelData> CreateModelDataFromGeometry(
			const std::vector<Vertex>& vertices,
			const std::vector<unsigned int>& indices,
			const glm::vec3& diffuseColor = glm::vec3(1.0f)
		);
	};

} // namespace Boidsish
