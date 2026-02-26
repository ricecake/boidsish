#pragma once

#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "model.h"

namespace Boidsish {

	enum class ProceduralType { Rock, Grass, Flower, Tree, TreeSpaceColonization };

	class ProceduralGenerator {
	public:
		static std::shared_ptr<Model> Generate(ProceduralType type, unsigned int seed);

		static std::shared_ptr<Model> GenerateRock(unsigned int seed);
		static std::shared_ptr<Model> GenerateGrass(unsigned int seed);
		static std::shared_ptr<Model>
		GenerateFlower(unsigned int seed, const std::string& axiom = "", const std::vector<std::string>& rules = {}, int iterations = 2);
		static std::shared_ptr<Model>
		GenerateTree(unsigned int seed, const std::string& axiom = "", const std::vector<std::string>& rules = {}, int iterations = 3);
		static std::shared_ptr<Model> GenerateSpaceColonizationTree(unsigned int seed);

	private:
		struct TurtleState {
			glm::vec3 position;
			glm::quat orientation;
			float     thickness;
		};

		static std::shared_ptr<ModelData> CreateModelDataFromGeometry(
			const std::vector<Vertex>&       vertices,
			const std::vector<unsigned int>& indices,
			const glm::vec3&                 diffuseColor = glm::vec3(1.0f)
		);
	};

} // namespace Boidsish
