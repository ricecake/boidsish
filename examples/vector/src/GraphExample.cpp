#include "GraphExample.h"

#include <cmath>

#include "graph.h"
#include "logger.h"

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

namespace Boidsish {

	std::vector<std::shared_ptr<Shape>> GraphExample(float time) {
		static std::shared_ptr<Graph> graph = nullptr;
		if (!graph) {
			graph = std::make_shared<Graph>(0, 0, 0, 0);
			// Initial setup
			auto root = graph->AddVertex(Vector3(0, 0, 0), 48.0f, 0, 0, 1, 1);
			auto trunk = graph->AddVertex(Vector3(0, 6, 0), 16.0f, 0, 1, 1, 1);
			root.Link(trunk);

			graph->AddVertex(Vector3(0, 11, 0), 24.0f).Link(trunk);
			graph->AddVertex(Vector3(3, 10, 0), 24.0f).Link(trunk);
			graph->AddVertex(Vector3(-3, 10, 0), 24.0f).Link(trunk);
			graph->AddVertex(Vector3(0, 10, 3), 24.0f).Link(trunk);
			graph->AddVertex(Vector3(0, 10, -3), 24.0f).Link(trunk);
		}

		// Update vertex positions and colors over time
		// Node 0: root, Node 1: trunk
		// Node 2-6: branches

		graph->V(2).position = Vector3(0, 11, 0);
		graph->V(2).r = fabs(sin(time / 2));
		graph->V(2).g = fabs(sin(time / 3 + M_PI / 3));
		graph->V(2).b = fabs(sin(time / 5 + (2 * M_PI / 3)));

		graph->V(3).position = Vector3(3, 10 + sin(time), cos(time));
		graph->V(3).r = fabs(sin(time / 2));
		graph->V(3).g = fabs(sin(time / 5 + (2 * M_PI / 3)));
		graph->V(3).b = fabs(sin(time / 3 + M_PI / 3));

		graph->V(4).position = Vector3(-3, 10 + sin(time), cos(time));
		graph->V(4).r = fabs(sin(time / 3 + (2 * M_PI / 3)));
		graph->V(4).g = fabs(sin(time / 2 + M_PI / 3));
		graph->V(4).b = fabs(sin(time / 5));

		graph->V(5).position = Vector3(cos(time), 10 + sin(time), 3);
		graph->V(5).r = fabs(sin(time / 3 + M_PI / 3));
		graph->V(5).g = fabs(sin(time / 5));
		graph->V(5).b = fabs(sin(time / 2 + (2 * M_PI / 3)));

		graph->V(6).position = Vector3(cos(time), 10 + sin(time), -3);
		graph->V(6).r = fabs(sin(time / 5 + (2 * M_PI / 3)));
		graph->V(6).g = fabs(sin(time / 3 + M_PI / 3));
		graph->V(6).b = fabs(sin(time / 2));

		std::vector<std::shared_ptr<Shape>> shapes;
		shapes.push_back(graph);
		return shapes;
	}

} // namespace Boidsish
