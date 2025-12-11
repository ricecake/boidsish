#pragma once

#include <functional>
#include <memory>
#include <vector>

class Shader;

namespace Boidsish {

	// Base class for all renderable shapes
	class Shape {
	public:
		virtual ~Shape() = default;

		// Pure virtual function for rendering the shape
		virtual void render() const = 0;

		// Accessors
		inline int GetId() const { return id_; }

		inline void SetId(int id) { id_ = id; }

		inline float GetX() const { return x_; }

		inline float GetY() const { return y_; }

		inline float GetZ() const { return z_; }

		inline void SetPosition(float x, float y, float z) {
			x_ = x;
			y_ = y;
			z_ = z;
		}

		inline float GetR() const { return r_; }

		inline float GetG() const { return g_; }

		inline float GetB() const { return b_; }

		inline float GetA() const { return a_; }

		inline void SetColor(float r, float g, float b, float a = 1.0f) {
			r_ = r;
			g_ = g;
			b_ = b;
			a_ = a;
		}

		inline int GetTrailLength() const { return trail_length_; }

		inline void SetTrailLength(int length) { trail_length_ = length; }

		// Static shader reference
		static std::shared_ptr<Shader> shader;

	protected:
		// Protected constructor for derived classes
		Shape(
			int   id = 0,
			float x = 0.0f,
			float y = 0.0f,
			float z = 0.0f,
			float r = 1.0f,
			float g = 1.0f,
			float b = 1.0f,
			float a = 1.0f,
			int   trail_length = 0
		):
			id_(id), x_(x), y_(y), z_(z), r_(r), g_(g), b_(b), a_(a), trail_length_(trail_length) {}

	private:
		int   id_;
		float x_, y_, z_;
		float r_, g_, b_, a_;
		int   trail_length_;
	};

	// Function type for user-defined shape generation
	using ShapeFunction = std::function<std::vector<std::shared_ptr<Shape>>(float time)>;
} // namespace Boidsish