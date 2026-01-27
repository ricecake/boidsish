#include <iostream>
#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"
#include "light.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Light Behaviors Demo");

		// 1. Blink (Red) - 1s period, 50% duty cycle
		auto blink_light = Boidsish::Light::Create({-15, 5, 0}, 10.0f, {1, 0, 0});
		blink_light.SetBlink(1.0f, 0.5f);
		vis.GetLightManager().AddLight(blink_light);

		// 2. Pulse (Green) - 2s period
		auto pulse_light = Boidsish::Light::Create({-7.5, 5, 0}, 10.0f, {0, 1, 0});
		pulse_light.SetPulse(2.0f, 1.0f);
		vis.GetLightManager().AddLight(pulse_light);

		// 3. Ease In-Out (Blue) - 3s period
		auto ease_light = Boidsish::Light::Create({0, 5, 0}, 10.0f, {0, 0, 1});
		ease_light.SetEaseInOut(3.0f);
		vis.GetLightManager().AddLight(ease_light);

		// 4. Flicker (Yellow) - Scary movie style
		auto flicker_light = Boidsish::Light::Create({7.5, 5, 0}, 10.0f, {1, 1, 0});
		flicker_light.SetFlicker(5.0f);
		vis.GetLightManager().AddLight(flicker_light);

		// 5. Morse Code (Cyan)
		auto morse_light = Boidsish::Light::Create({15, 5, 0}, 15.0f, {0, 1, 1});
		morse_light.SetMorse("HELP I AM TRAPPED", 0.15f);
		vis.GetLightManager().AddLight(morse_light);

		vis.AddShapeHandler([&](float) {
			std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
			// Grid of dots to see lighting effect
			for (float x = -20; x <= 20; x += 2.0f) {
				for (float z = -5; z <= 5; z += 2.0f) {
					auto dot = std::make_shared<Boidsish::Dot>(static_cast<int>(x * 100 + z), x, 0.0f, z, 1.0f);
					dot->SetColor(0.8f, 0.8f, 0.8f);
					dot->SetUsePBR(true);
					dot->SetRoughness(0.4f);
					dot->SetMetallic(0.0f);
					shapes.push_back(dot);
				}
			}
			return shapes;
		});

		vis.GetCamera().z = 20.0f;
		vis.GetCamera().pitch = -15.0f;

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
