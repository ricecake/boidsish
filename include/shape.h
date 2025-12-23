#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "visual_effects.h"
#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

	// Base class for all renderable shapes
	class Shape {
	public:
		virtual ~Shape() = default;

		// Pure virtual function for rendering the shape
		virtual void render() const = 0;

		// Get the active visual effects for this shape
		virtual std::vector<VisualEffect> GetActiveEffects() const { return {}; }

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

		// Sphere mesh generation
		static void InitSphereMesh();
		static void DestroySphereMesh();
		static void RenderSphere(const glm::vec3& position, const glm::vec3& color, float scale);

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

	private:
		// Shared sphere mesh
		static unsigned int sphere_vao_;
		static unsigned int sphere_vbo_;
		static int          sphere_vertex_count_;
	};

} // namespace Boidsish