#pragma once

#include "shape.h"
#include "vector.h"
#include <GL/glew.h>

namespace Boidsish {

	// Class representing a single dot/particle
	class Dot: public Shape {
	public:
		// Constructor with proper parameter names
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

		// Size accessor/mutator
		inline float GetSize() const { return size_; }

		inline void SetSize(float size) { size_ = size; }

		// Render implementation
		void render() const override;

	private:
		float size_;
	};
} // namespace Boidsish