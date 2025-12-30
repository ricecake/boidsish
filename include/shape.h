#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "visual_effects.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Shader;

namespace Boidsish {

	// Base class for all renderable shapes
	class Shape {
	public:
		virtual ~Shape() = default;

		// Pure virtual function for rendering the shape
		virtual void render() const = 0;

		virtual void render(Shader& shader) const final { render(shader, GetModelMatrix()); }

		virtual void      render(Shader& shader, const glm::mat4& model_matrix) const = 0;
		virtual glm::mat4 GetModelMatrix() const = 0;

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

		inline const glm::quat& GetRotation() const { return rotation_; }

		inline void SetRotation(const glm::quat& rotation) { rotation_ = rotation; }

		void LookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

		inline const glm::vec3& GetScale() const { return scale_; }

		inline void SetScale(const glm::vec3& scale) { scale_ = scale; }

		inline int GetTrailLength() const { return trail_length_; }

		inline void SetTrailLength(int length) { trail_length_ = length; }

		inline bool IsTrailIridescent() const { return trail_iridescent_; }

		inline void SetTrailIridescence(bool enabled) { trail_iridescent_ = enabled; }

		// New rocket trail property
		inline bool IsTrailRocket() const { return trail_rocket_; }

		inline void SetTrailRocket(bool enabled) { trail_rocket_ = enabled; }

		// Static shader reference
		static std::shared_ptr<Shader> shader;

		// Sphere mesh generation
		static void InitSphereMesh();
		static void DestroySphereMesh();
		static void RenderSphere(
			const glm::vec3& position,
			const glm::vec3& color,
			const glm::vec3& scale,
			const glm::quat& rotation
		);

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
			id_(id),
			x_(x),
			y_(y),
			z_(z),
			r_(r),
			g_(g),
			b_(b),
			a_(a),
			rotation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
			scale_(glm::vec3(1.0f)),
			trail_length_(trail_length),
			trail_iridescent_(false) {}

		glm::quat rotation_;
		glm::vec3 scale_;

	private:
		int   id_;
		float x_, y_, z_;
		float r_, g_, b_, a_;
		int   trail_length_;
		bool  trail_iridescent_;
		bool  trail_rocket_;

	protected:
		// Shared sphere mesh
		static unsigned int sphere_vao_;
		static unsigned int sphere_vbo_;
		static int          sphere_vertex_count_;
	};

	// Function type for user-defined shape generation
	using ShapeFunction = std::function<std::vector<std::shared_ptr<Shape>>(float time)>;
} // namespace Boidsish