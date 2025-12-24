#include "graphics.h"
#include "graph_handler.h"
#include <iostream>

using namespace Boidsish;

int main(int argc, char** argv) {
    try {
        Visualizer vis;

        auto graph_handler = std::make_shared<GraphHandler>();
        auto graph = graph_handler->AddGraph();

        auto& v1 = graph->AddVertex({-5, 0, 0}, 10.0f, 1, 0, 0, 1);
        auto& v2 = graph->AddVertex({5, 0, 0}, 10.0f, 0, 1, 0, 1);
        auto& v3 = graph->AddVertex({0, 5, 0}, 5.0f, 0, 0, 1, 1);

        v1.Link(v2);
        v1.Link(v3);
        v2.Link(v3);

        vis.AddShapeHandler([graph_handler](float time) {
            return graph_handler->GetShapes();
        });

        vis.SetCameraMode(CameraMode::AUTO);
        vis.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
