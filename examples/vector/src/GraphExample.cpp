#include "GraphExample.h"

#include <cmath>

#include "graph.h"
#include "logger.h"

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

namespace Boidsish {

	std::vector<std::shared_ptr<Shape>> GraphExample(float time) {
		std::vector<std::shared_ptr<Shape>> shapes;
		auto                                graph = std::make_shared<Graph>(0, 0, 0);

		auto root = graph->AddVertex(Vector3(0, 0, 0), 48.0f, 0, 0, 1, 1);
		auto trunk = graph->AddVertex(Vector3(0, 6, 0), 16.0f, 0, 1, 1, 1);
		root.Link(trunk);

		graph
			->AddVertex(
				Vector3(0, 11, 0),
				24.0f,
				fabs(sin(time / 2)),
				fabs(sin(time / 3 + M_PI / 3)),
				fabs(sin(time / 5 + (2 * M_PI / 3))),
				1
			)
			.Link(trunk);
		graph
			->AddVertex(
				Vector3(3, 10 + sin(time), cos(time)),
				24.0f,
				fabs(sin(time / 2)),
				fabs(sin(time / 5 + (2 * M_PI / 3))),
				fabs(sin(time / 3 + M_PI / 3)),
				1
			)
			.Link(trunk);
		graph
			->AddVertex(
				Vector3(-3, 10 + sin(time), cos(time)),
				24.0f,
				fabs(sin(time / 3 + (2 * M_PI / 3))),
				fabs(sin(time / 2 + M_PI / 3)),
				fabs(sin(time / 5)),
				1
			)
			.Link(trunk);
		graph
			->AddVertex(
				Vector3(cos(time), 10 + sin(time), 3),
				24.0f,
				fabs(sin(time / 3 + M_PI / 3)),
				fabs(sin(time / 5)),
				fabs(sin(time / 2 + (2 * M_PI / 3))),
				1
			)
			.Link(trunk);
		graph
			->AddVertex(
				Vector3(cos(time), 10 + sin(time), -3),
				24.0f,
				fabs(sin(time / 5 + (2 * M_PI / 3))),
				fabs(sin(time / 3 + M_PI / 3)),
				fabs(sin(time / 2)),
				1
			)
			.Link(trunk);

		shapes.push_back(graph);
		return shapes;
	}

} // namespace Boidsish
