#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace Boidsish {

	// Base class for all renderable shapes
	class Shape {
	public:
		virtual ~Shape() = default;

		// Pure virtual function for rendering the shape
		virtual void render() const = 0;

		// Common properties
		int   id;
		float x, y, z;
		float r, g, b, a;
		int   trail_length;
	};

	// Function type for user-defined shape generation
	using ShapeFunction = std::function<std::vector<std::shared_ptr<Shape>>(float time)>;
}