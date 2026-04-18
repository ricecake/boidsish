#include "graphics.h"
#include "plant_manager.h"
#include "logger.h"
#include <iostream>

using namespace Boidsish;

int main() {
    try {
        Visualizer visualizer(1280, 720, "Alien Plant Demo");

        visualizer.AddPrepareCallback([](Visualizer& v) {
            auto plantManager = v.GetPlantManager();
            if (plantManager) {
                PlantProperties props;
                props.tubeColor = glm::vec4(0.5f, 0.2f, 0.8f, 1.0f);
                props.ballColor = glm::vec4(0.9f, 0.4f, 0.1f, 1.0f);
                props.grassColor = glm::vec4(0.2f, 0.9f, 0.4f, 1.0f);
                props.tubeRadius = 0.2f;
                props.tubeLength = 4.0f;
                props.ballRadius = 0.5f;
                props.grassDensity = 2.0f;
                props.curveStrength = 0.8f;
                props.spiralFrequency = 3.0f;
                props.zigZagStrength = 0.3f;

                plantManager->GetProperties() = props;
                plantManager->MarkDirty();

                logger::LOG("Alien Plant properties initialized");
            }
        });

        visualizer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
