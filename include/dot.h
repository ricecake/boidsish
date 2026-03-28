#pragma once

#include <string>

#include "constants.h"
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
		    int   trail_length = Constants::Class::Trails::DefaultTrailLength());

		bool GetDefaultCastsShadows() const override { return false; }

		float GetSizeMultiplier() const override { return 0.01f; }

		bool Intersects(const Ray& ray, float& t) const override {
			glm::vec3 center(GetX(), GetY(), GetZ());
			float     radius = GetSize() * GetSizeMultiplier();
			glm::vec3 oc = ray.origin - center;
			float     a = glm::dot(ray.direction, ray.direction);
			float     b = 2.0f * glm::dot(oc, ray.direction);
			float     c = glm::dot(oc, oc) - radius * radius;
			float     discriminant = b * b - 4 * a * c;

			if (discriminant < 0) {
				return false;
			} else {
				t = (-b - sqrt(discriminant)) / (2.0f * a);
				return t > 0;
			}
		}

		// All Dots share the same sphere mesh, so they can be instanced together
		std::string GetInstanceKey() const override { return "Dot"; }
	};
} // namespace Boidsish
