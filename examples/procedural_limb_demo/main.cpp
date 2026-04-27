#include <memory>
#include <vector>

#include "decor_manager.h"
#include "graphics.h"
#include "service_locator.h"
#include "procedural_generator.h"
#include "procedural_mesher.h"
#include "procedural_optimizer.h"
#include "procedural_refiner.h"
#include "terrain_generator.h"

using namespace Boidsish;

int main(int argc, char** argv) {
	Visualizer vis(1280, 960, "Procedural Limb Test");

	// Generate a tree with explicitly different stiffness for limbs
	// This is a proof-of-concept for the new vertex attributes.
	ProceduralIR ir;
	ir.name = "limb_test";

	// Trunk
	int trunk = ir.AddTube({0,0,0}, {0,2,0}, 0.2f, 0.15f, {0.4f, 0.3f, 0.1f}, -1, "trunk", true, SkinningMode::Smooth, 1.0f);

	// Branch 1 - Stiff
	int b1 = ir.AddTube({0,2,0}, {1,3,0}, 0.15f, 0.12f, {0.45f, 0.35f, 0.15f}, trunk, "branch_1", true, SkinningMode::Smooth, 1.2f);
	// Sub-limbs for Branch 1
	ir.AddTube({1,3,0}, {1.5,3.5,0.5}, 0.12f, 0.08f, {0.5f, 0.4f, 0.2f}, b1, "b1_limb1", true, SkinningMode::Smooth, 0.8f);
	ir.AddTube({1,3,0}, {1.5,3.5,-0.5}, 0.12f, 0.08f, {0.5f, 0.4f, 0.2f}, b1, "b1_limb2", true, SkinningMode::Smooth, 0.8f);

	// Branch 2 - Flexible
	int b2 = ir.AddTube({0,2,0}, {-1,3,0}, 0.15f, 0.12f, {0.45f, 0.35f, 0.15f}, trunk, "branch_2", true, SkinningMode::Smooth, 0.5f);
	// Sub-limbs for Branch 2
	ir.AddTube({-1,3,0}, {-1.5,3.5,0.5}, 0.12f, 0.08f, {0.5f, 0.4f, 0.2f}, b2, "b2_limb1", true, SkinningMode::Smooth, 1.5f);
	ir.AddTube({-1,3,0}, {-1.5,3.5,-0.5}, 0.12f, 0.08f, {0.5f, 0.4f, 0.2f}, b2, "b2_limb2", true, SkinningMode::Smooth, 1.5f);

	// Optimize and Refine
	ProceduralOptimizer::Optimize(ir);
	ProceduralRefiner::Refine(ir);
	auto model = ProceduralMesher::GenerateModel(ir);

	ServiceLocator loc;
	auto decor_manager = std::make_shared<DecorManager>(loc);

	DecorProperties props;
	props.SetDensity(2.0f);
	props.biomes = {Biome::LushGrass, Biome::Forest, Biome::AlpineMeadow};
	decor_manager->AddDecorType(model, props);

	vis.SetDecorManager(decor_manager);

	vis.Run();

	return 0;
}
