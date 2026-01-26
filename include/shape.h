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

		inline glm::vec3 GetLastPosition() const { return last_position_; }

		inline void UpdateLastPosition() { last_position_ = glm::vec3(x_, y_, z_); }

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

		// Trail PBR properties
		inline bool GetTrailPBR() const { return trail_pbr_; }

		inline void SetTrailPBR(bool enabled) { trail_pbr_ = enabled; }

		inline float GetTrailRoughness() const { return trail_roughness_; }

		inline void SetTrailRoughness(float roughness) { trail_roughness_ = glm::clamp(roughness, 0.0f, 1.0f); }

		inline float GetTrailMetallic() const { return trail_metallic_; }

		inline void SetTrailMetallic(float metallic) { trail_metallic_ = glm::clamp(metallic, 0.0f, 1.0f); }

		inline bool IsColossal() const { return is_colossal_; }

		inline void SetColossal(bool is_colossal) { is_colossal_ = is_colossal; }

		inline bool IsInstanced() const { return is_instanced_; }

		inline void SetInstanced(bool is_instanced) { is_instanced_ = is_instanced; }

		// PBR material properties
		inline float GetRoughness() const { return roughness_; }

		inline void SetRoughness(float roughness) { roughness_ = glm::clamp(roughness, 0.0f, 1.0f); }

		inline float GetMetallic() const { return metallic_; }

		inline void SetMetallic(float metallic) { metallic_ = glm::clamp(metallic, 0.0f, 1.0f); }

		inline float GetAO() const { return ao_; }

		inline void SetAO(float ao) { ao_ = glm::clamp(ao, 0.0f, 1.0f); }

		inline bool UsePBR() const { return use_pbr_; }

		inline void SetUsePBR(bool use_pbr) { use_pbr_ = use_pbr; }

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
			trail_iridescent_(false),
			is_colossal_(false),
			last_position_(x, y, z),
			trail_pbr_(false),
			trail_roughness_(0.3f),
			trail_metallic_(0.0f),
			roughness_(0.5f),
			metallic_(0.0f),
			ao_(1.0f),
			use_pbr_(false) {}

		glm::quat rotation_;
		glm::vec3 scale_;

	private:
		int       id_;
		float     x_, y_, z_;
		glm::vec3 last_position_;
		float     r_, g_, b_, a_;
		int       trail_length_;
		bool      trail_iridescent_;
		bool      trail_rocket_;
		bool      is_colossal_;
		bool      is_instanced_ = false;
		bool      trail_pbr_;
		float     trail_roughness_;
		float     trail_metallic_;
		float     roughness_;
		float     metallic_;
		float     ao_;
		bool      use_pbr_;

	protected:
		// Shared sphere mesh
		static unsigned int sphere_vao_;
		static unsigned int sphere_vbo_;
		static int          sphere_vertex_count_;
	};

	// Function type for user-defined shape generation
	using ShapeFunction = std::function<std::vector<std::shared_ptr<Shape>>(float time)>;
} // namespace Boidsish