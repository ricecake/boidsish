#include <memory>
#include <vector>

#include "decor_manager.h"
#include "graphics.h"
#include "service_locator.h"
#include "procedural_generator.h"
#include "terrain_generator.h"

using namespace Boidsish;

int main(int argc, char** argv) {
	Visualizer vis(1280, 960, "Procedural Models Test");

	auto rock = ProceduralGenerator::GenerateRock(123);
	auto grass = ProceduralGenerator::GenerateGrass(4556);
	auto flower1 = ProceduralGenerator::GenerateFlower(789);
	auto flower2 = ProceduralGenerator::GenerateFlower(987);
	auto tree1 = ProceduralGenerator::GenerateTree(101112);
	auto tree2 = ProceduralGenerator::GenerateTree(211101);

	// Add more instances of the same model to show variety
	std::vector<std::shared_ptr<Model>> many_trees;
	for (int i = 0; i < 10; ++i) {
		auto t = std::make_shared<Model>(tree1->GetData());
		t->SetPosition(-20.0f + i * 4.0f, 0.0f, -10.0f);
		vis.AddShape(t);
	}

	ServiceLocator loc;
	auto decor_manager = std::make_shared<DecorManager>(loc);

	// // Add procedural models to decor manager
	// DecorProperties rock_props = DecorManager::GetDefaultRockProperties();
	// rock_props.base_scale = 1.0f;
	// decor_manager->AddDecorType(rock, rock_props);

	DecorProperties grass_props;
	grass_props.min_height = 0.1f;
	grass_props.wind_responsiveness = 1.2f;
	grass_props.wind_rim_highlight = 1.1f;
	grass_props.SetDensity(1.0f);
	grass_props.biomes = {Biome::LushGrass, Biome::Forest};
	grass_props.align_to_terrain = true;
	decor_manager->AddDecorType(grass, grass_props);

	DecorProperties flower_props;
	flower_props.SetDensity(0.1f);
	flower_props.biomes = {Biome::LushGrass, Biome::AlpineMeadow};
	decor_manager->AddDecorType(flower1, flower_props);
	decor_manager->AddDecorType(flower2, flower_props);

	DecorProperties tree_props = DecorManager::GetDefaultTreeProperties();
	tree_props.base_scale = 1.0f;
	decor_manager->AddDecorType(tree1, tree_props);
	decor_manager->AddDecorType(tree2, tree_props);

	vis.SetDecorManager(decor_manager);

	vis.Run();

	return 0;
}
