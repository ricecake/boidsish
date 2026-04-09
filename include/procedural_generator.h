#pragma once

#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "model.h"
#include "procedural_ir.h"

namespace Boidsish {

	class Visualizer;

	enum class ProceduralType {
		Rock,
		Grass,
		Flower,
		Tree,
		TreeSpaceColonization,
		TreeSpring,
		Critter,
		Structure
	};

	struct SpringPlantConfig {
		float spring_repulsion = 1.0f;
		float ground_repulsion = 1.0f;
		float branch_length_factor = 1.2f;
		int   iterations = 5;
		float size_limit = 15.0f;
		float equilibrium_time = 1.5f; // Seconds per generation
		float branch_split_factor = 0.7f;
		float up_pull = 1.0f;
		float curvature = 0.1f;
		float spiral = 0.1f;
		int   min_branches = 2;
		int   max_branches = 3;
	};

	class ProceduralGenerator {
	public:
		static std::shared_ptr<Model> Generate(ProceduralType type, unsigned int seed);

		static std::shared_ptr<Model> GenerateRock(unsigned int seed);
		static std::shared_ptr<Model> GenerateGrass(unsigned int seed);
		static std::shared_ptr<Model> GenerateFlower(
			unsigned int                    seed,
			const std::string&              axiom = "",
			const std::vector<std::string>& rules = {},
			int                             iterations = 2
		);
		static std::shared_ptr<Model> GenerateTree(
			unsigned int                    seed,
			const std::string&              axiom = "",
			const std::vector<std::string>& rules = {},
			int                             iterations = 3
		);
		static std::shared_ptr<Model> GenerateSpaceColonizationTree(unsigned int seed);
		static std::shared_ptr<Model> GenerateSpringPlant(unsigned int seed, const SpringPlantConfig& config = {});
		static std::shared_ptr<Model> GenerateCritter(
			unsigned int                    seed,
			const std::string&              axiom = "",
			const std::vector<std::string>& rules = {},
			int                             iterations = 3
		);

		static ProceduralIR GenerateGrassIR(unsigned int seed);
		static ProceduralIR GenerateFlowerIR(
			unsigned int                    seed,
			const std::string&              axiom = "",
			const std::vector<std::string>& rules = {},
			int                             iterations = 2
		);
		static ProceduralIR GenerateTreeIR(
			unsigned int                    seed,
			const std::string&              axiom = "",
			const std::vector<std::string>& rules = {},
			int                             iterations = 3
		);
		static ProceduralIR GenerateSpaceColonizationTreeIR(unsigned int seed);
		static ProceduralIR GenerateSpringPlantIR(unsigned int seed, const SpringPlantConfig& config = {});
		static ProceduralIR GenerateCritterIR(
			unsigned int                    seed,
			const std::string&              axiom = "",
			const std::vector<std::string>& rules = {},
			int                             iterations = 3
		);
		static ProceduralIR GenerateStructureIR(unsigned int seed);

		/**
		 * @brief Helper to level the terrain underneath a model's footprint.
		 * Uses FlattenSquareDeformation to create a flat area at the model's base.
		 */
		static void LevelTerrainForModel(Visualizer& viz, std::shared_ptr<Model> model, float blend_distance = 2.0f);

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
