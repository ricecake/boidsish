#include <memory>
#include <vector>

#include "graphics.h"
#include "decor_manager.h"
#include "procedural_generator.h"
#include "terrain_generator.h"

using namespace Boidsish;

int main(int argc, char** argv) {
	Visualizer vis(1280, 960, "Procedural Models Test");

	auto rock = ProceduralGenerator::GenerateRock(123);
	auto grass = ProceduralGenerator::GenerateGrass(456);
	auto flower = ProceduralGenerator::GenerateFlower(789);
	auto tree = ProceduralGenerator::GenerateTree(101112);

	auto decor_manager = std::make_shared<DecorManager>();

	// Add procedural models to decor manager
	DecorProperties rock_props = DecorManager::GetDefaultRockProperties();
	rock_props.base_scale = 1.0f; // Adjusted for procedural scale
	decor_manager->AddDecorType(rock, rock_props);

	DecorProperties grass_props;
	grass_props.SetDensity(0.5f);
	grass_props.biomes = {Biome::LushGrass, Biome::Forest};
	grass_props.align_to_terrain = true;
	decor_manager->AddDecorType(grass, grass_props);

	DecorProperties flower_props;
	flower_props.SetDensity(0.2f);
	flower_props.biomes = {Biome::LushGrass, Biome::AlpineMeadow};
	decor_manager->AddDecorType(flower, flower_props);

	DecorProperties tree_props = DecorManager::GetDefaultTreeProperties();
	tree_props.base_scale = 1.0f; // Adjusted for procedural scale
	decor_manager->AddDecorType(tree, tree_props);

	vis.SetDecorManager(decor_manager);

	vis.Run();

	return 0;
}
