#include "graphics.h"
#include "grass_manager.h"
#include "logger.h"
#include <iostream>

using namespace Boidsish;

int main() {
    try {
        Visualizer visualizer(1280, 720, "Grass System Demo");

        visualizer.AddPrepareCallback([](Visualizer& v) {
            auto grassManager = v.GetGrassManager();
            if (grassManager) {
                // Lush Grass
                GrassProperties lushGrass;
                lushGrass.colorTop = glm::vec4(0.3f, 0.8f, 0.2f, 1.0f);
                lushGrass.colorBottom = glm::vec4(0.1f, 0.3f, 0.05f, 1.0f);
                lushGrass.height = 1.0f;
                lushGrass.width = 0.1f;
                lushGrass.density = 1.0f;
                lushGrass.windInfluence = 1.0f;
                lushGrass.rigidity = 0.3f;
                grassManager->SetGrassProperties(Biome::LushGrass, lushGrass);

                // Dry Grass
                GrassProperties dryGrass;
                dryGrass.colorTop = glm::vec4(0.7f, 0.6f, 0.3f, 1.0f);
                dryGrass.colorBottom = glm::vec4(0.3f, 0.25f, 0.1f, 1.0f);
                dryGrass.height = 0.8f;
                dryGrass.width = 0.08f;
                dryGrass.density = 0.7f;
                dryGrass.windInfluence = 0.6f;
                dryGrass.rigidity = 0.6f;
                grassManager->SetGrassProperties(Biome::DryGrass, dryGrass);

                // Forest Grass
                GrassProperties forestGrass;
                forestGrass.colorTop = glm::vec4(0.1f, 0.4f, 0.1f, 1.0f);
                forestGrass.colorBottom = glm::vec4(0.02f, 0.1f, 0.02f, 1.0f);
                forestGrass.height = 1.5f;
                forestGrass.width = 0.12f;
                forestGrass.density = 0.9f;
                forestGrass.windInfluence = 0.4f;
                forestGrass.rigidity = 0.5f;
                grassManager->SetGrassProperties(Biome::Forest, forestGrass);

                // Alpine Meadow Grass
                GrassProperties alpineGrass;
                alpineGrass.colorTop = glm::vec4(0.4f, 0.9f, 0.4f, 1.0f);
                alpineGrass.colorBottom = glm::vec4(0.1f, 0.4f, 0.1f, 1.0f);
                alpineGrass.height = 0.6f;
                alpineGrass.width = 0.06f;
                alpineGrass.density = 1.0f;
                alpineGrass.windInfluence = 1.2f;
                alpineGrass.rigidity = 0.2f;
                grassManager->SetGrassProperties(Biome::AlpineMeadow, alpineGrass);

                logger::LOG("Biome-specific grass properties added to GrassManager");
            }
        });

        visualizer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
