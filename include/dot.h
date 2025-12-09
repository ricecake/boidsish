#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "vector.h"
#include "shape.h"

namespace Boidsish {

	// Class representing a single dot/particle, inheriting from Shape
	class Dot: public Shape {
	public:
		float size; // Size of the dot

		Dot(int   id = 0,
		    float x = 0.0f,
		    float y = 0.0f,
		    float z = 0.0f,
		    float size = 1.0f,
		    float r = 1.0f,
		    float g = 1.0f,
		    float b = 1.0f,
		    float a = 1.0f,
		    int   trail_length = 10);

		void render() const override;
	};
}