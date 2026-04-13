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
                GrassProperties lushGrass;
                lushGrass.colorTop = glm::vec4(0.3f, 0.8f, 0.2f, 1.0f);
                lushGrass.colorBottom = glm::vec4(0.1f, 0.3f, 0.05f, 1.0f);
                lushGrass.height = 1.2f;
                lushGrass.width = 0.15f;
                lushGrass.density = 1.0f;
                lushGrass.windInfluence = 0.8f;
                lushGrass.biomeMask = (1u << static_cast<uint32_t>(Biome::LushGrass)) |
                                      (1u << static_cast<uint32_t>(Biome::AlpineMeadow));

                grassManager->AddGrassType("Lush Grass", lushGrass);

                logger::LOG("Grass types added to GrassManager");
            }
        });

        visualizer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
