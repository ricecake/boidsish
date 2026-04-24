#include <memory>
#include <vector>
#include <cmath>

#include "graphics.h"
#include "procedural_generator.h"
#include "model.h"

using namespace Boidsish;

int main(int argc, char** argv) {
	Visualizer vis(1280, 960, "Variety Demo");

	// Generate a single source tree model
	auto tree_data = ProceduralGenerator::GenerateTree(12345)->GetData();

	// 1. Reference Tree (No variety)
	auto ref_tree = std::make_shared<Model>(tree_data);
	ref_tree->SetPosition(-15.0f, 0.0f, 0.0f);
	ref_tree->SetVarietyAmount(0.0f);
	vis.AddShape(ref_tree);

	// 2. High Variety Trees (Static)
	for (int i = 0; i < 5; ++i) {
		auto v_tree = std::make_shared<Model>(tree_data);
		v_tree->SetPosition(-5.0f + i * 5.0f, 0.0f, 0.0f);
		v_tree->SetVarietySeed((float)i * 123.456f);
		v_tree->SetVarietyAmount(0.6f); // 60% variation in limb length/bending
		vis.AddShape(v_tree);
	}

	// 3. Pulsing Variety Tree (Animated)
	auto pulse_tree = std::make_shared<Model>(tree_data);
	pulse_tree->SetPosition(0.0f, 0.0f, 15.0f);
	vis.AddShape(pulse_tree);

	vis.AddUpdateHandler([pulse_tree](float time, float dt) {
		// Pulse variety amount between 0 and 1 over 4 seconds
		float pulse = (std::sin(time * 1.5f) * 0.5f + 0.5f);
		pulse_tree->SetVarietyAmount(pulse);
	});

	vis.Run();

	return 0;
}
