#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "collision.h"
#include "constants.h"
#include "geometry.h"
#include "visual_effects.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Shader;

namespace Boidsish {

	// Base class for all renderable shapes
	class Shape: public Geometry {
	public:
		virtual ~Shape() = default;

		// Move operations (std::atomic is not copyable, so we need explicit move semantics)
		Shape(Shape&& other) noexcept:
			Geometry(std::move(other)),
			rotation_(std::move(other.rotation_)),
			scale_(std::move(other.scale_)),
			local_aabb_(std::move(other.local_aabb_)),
			clamp_to_terrain_(other.clamp_to_terrain_),
			ground_offset_(other.ground_offset_),
			render_dirty_(other.render_dirty_.load(std::memory_order_relaxed)),
			cached_packets_(std::move(other.cached_packets_)),
			id_(other.id_),
			x_(other.x_),
			y_(other.y_),
			z_(other.z_),
			last_position_(other.last_position_),
			r_(other.r_),
			g_(other.g_),
			b_(other.b_),
			a_(other.a_),
			trail_length_(other.trail_length_),
			trail_thickness_(other.trail_thickness_),
			trail_iridescent_(other.trail_iridescent_),
			trail_rocket_(other.trail_rocket_),
			is_colossal_(other.is_colossal_),
			is_hidden_(other.is_hidden_),
			trail_pbr_(other.trail_pbr_),
			trail_roughness_(other.trail_roughness_),
			trail_metallic_(other.trail_metallic_),
			roughness_(other.roughness_),
			metallic_(other.metallic_),
			ao_(other.ao_),
			use_pbr_(other.use_pbr_),
			dissolve_enabled_(other.dissolve_enabled_),
			dissolve_plane_normal_(other.dissolve_plane_normal_),
			dissolve_plane_dist_(other.dissolve_plane_dist_) {}

		Shape& operator=(Shape&& other) noexcept {
			if (this != &other) {
				Geometry::operator=(std::move(other));
				rotation_ = std::move(other.rotation_);
				scale_ = std::move(other.scale_);
				local_aabb_ = std::move(other.local_aabb_);
				clamp_to_terrain_ = other.clamp_to_terrain_;
				ground_offset_ = other.ground_offset_;
				render_dirty_.store(other.render_dirty_.load(std::memory_order_relaxed), std::memory_order_relaxed);
				cached_packets_ = std::move(other.cached_packets_);
				id_ = other.id_;
				x_ = other.x_;
				y_ = other.y_;
				z_ = other.z_;
				last_position_ = other.last_position_;
				r_ = other.r_;
				g_ = other.g_;
				b_ = other.b_;
				a_ = other.a_;
				trail_length_ = other.trail_length_;
				trail_thickness_ = other.trail_thickness_;
				trail_iridescent_ = other.trail_iridescent_;
				trail_rocket_ = other.trail_rocket_;
				is_colossal_ = other.is_colossal_;
				is_hidden_ = other.is_hidden_;
				trail_pbr_ = other.trail_pbr_;
				trail_roughness_ = other.trail_roughness_;
				trail_metallic_ = other.trail_metallic_;
				roughness_ = other.roughness_;
				metallic_ = other.metallic_;
				ao_ = other.ao_;
				use_pbr_ = other.use_pbr_;
				dissolve_enabled_ = other.dissolve_enabled_;
				dissolve_plane_normal_ = other.dissolve_plane_normal_;
				dissolve_plane_dist_ = other.dissolve_plane_dist_;
			}
			return *this;
		}

		// Delete copy operations (atomic is not copyable)
		Shape(const Shape&) = delete;
		Shape& operator=(const Shape&) = delete;

		// Update the shape's state
		virtual void Update(float delta_time) { (void)delta_time; }

		/**
		 * @brief Prepares any GPU resources needed for rendering.
		 * Called on the main thread before packet generation.
		 */
		virtual void PrepareResources(Megabuffer* megabuffer = nullptr) const { (void)megabuffer; }

		/// @name Dirty Flag Pattern (Thread Safety)
		/// @{
		/// Thread-safety contract:
		/// - IsDirty/MarkClean/MarkDirty are thread-safe (atomic flag)
		/// - Shape property modifications (SetPosition, SetColor, etc.) should be done
		///   on the main thread BEFORE parallel packet generation begins
		/// - GetCachedPackets/CachePackets are called from worker threads during packet
		///   generation; each shape is processed by exactly one worker thread
		bool IsDirty() const override { return render_dirty_.load(std::memory_order_acquire); }

		void MarkClean() const override { render_dirty_.store(false, std::memory_order_release); }

		void MarkDirty() override { render_dirty_.store(true, std::memory_order_release); }

		std::vector<RenderPacket>* GetCachedPackets() override {
			return render_dirty_.load(std::memory_order_acquire) ? nullptr : &cached_packets_;
		}

		void CachePackets(std::vector<RenderPacket>&& packets) override { cached_packets_ = std::move(packets); }

		/// @}

		// Check if the shape has expired (for transient effects)
		virtual bool IsExpired() const { return false; }

		// Implementation of Geometry interface
		virtual void
		GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const override;

		// Pure virtual function for legacy immediate rendering the shape
		virtual void render() const = 0;

		virtual void render(Shader& shader) const final { render(shader, GetModelMatrix()); }

		virtual void      render(Shader& shader, const glm::mat4& model_matrix) const = 0;
		virtual glm::mat4 GetModelMatrix() const = 0;

		// Get the active visual effects for this shape
		virtual std::vector<VisualEffect> GetActiveEffects() const { return {}; }

		virtual void GetGeometry(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) const;

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
			MarkDirty();
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
			MarkDirty();
		}

		inline const glm::quat& GetRotation() const { return rotation_; }

		inline void SetRotation(const glm::quat& rotation) {
			rotation_ = rotation;
			MarkDirty();
		}

		void LookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

		inline const glm::vec3& GetScale() const { return scale_; }

		inline void SetScale(const glm::vec3& scale) {
			scale_ = scale;
			MarkDirty();
		}

		/**
		 * @brief Scales the shape uniformly so its dimension on the specified axis matches max_dim.
		 *
		 * @param max_dim The target dimension for the specified axis
		 * @param axis The axis index (0=X, 1=Y, 2=Z)
		 */
		void SetScaleToMaxDimension(float max_dim, int axis);

		/**
		 * @brief Scales the shape uniformly so its dimension on the specified axis is a ratio
		 * of another shape's dimension on the same axis.
		 *
		 * @param other The reference shape
		 * @param ratio The scale factor relative to the other shape
		 * @param axis The axis index (0=X, 1=Y, 2=Z)
		 */
		void SetScaleRelativeTo(const Shape& other, float ratio, int axis);

		/**
		 * @brief Scales the shape uniformly to fit entirely inside another shape's AABB.
		 *
		 * @param other The shape to fit inside
		 */
		void SetScaleToFitInside(const Shape& other);

		inline int GetTrailLength() const { return trail_length_; }

		inline void SetTrailLength(int length) { trail_length_ = length; }

		inline float GetTrailThickness() const { return trail_thickness_; }

		inline void SetTrailThickness(float Thickness) { trail_thickness_ = Thickness; }

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

		inline void SetColossal(bool is_colossal) {
			is_colossal_ = is_colossal;
			MarkDirty();
		}

		virtual bool CastsShadows() const { return !is_colossal_; }

		inline bool IsHidden() const { return is_hidden_; }

		inline void SetHidden(bool hidden) {
			is_hidden_ = hidden;
			MarkDirty();
		}

		// Returns a key identifying what shapes can be instanced together
		// Shapes with the same key share the same mesh data
		virtual std::string GetInstanceKey() const = 0;

		/**
		 * @brief Indicates if the shape has transparent components and should be
		 * rendered in the transparent pass.
		 *
		 * @return true if the shape should be rendered with blending after opaque objects
		 */
		virtual bool IsTransparent() const { return a_ < 0.99f; }

		/**
		 * @brief Returns the bounding radius of the shape for frustum culling.
		 *
		 * @return float bounding radius in world units
		 */
		virtual float GetBoundingRadius() const { return 5.0f; }

		/**
		 * @brief Test for intersection with a ray.
		 *
		 * @param ray The ray to test against
		 * @param t Output: distance along the ray to the intersection point
		 * @return true if the ray intersects the shape
		 */
		virtual bool Intersects(const Ray& ray, float& t) const;

		/**
		 * @brief Get the world-space axis-aligned bounding box (AABB) for this shape.
		 *
		 * @return AABB in world coordinates
		 */
		virtual AABB GetAABB() const;

		// Terrain clamping
		inline bool IsClampedToTerrain() const { return clamp_to_terrain_; }

		inline void SetClampedToTerrain(bool clamp) { clamp_to_terrain_ = clamp; }

		inline float GetGroundOffset() const { return ground_offset_; }

		inline void SetGroundOffset(float offset) { ground_offset_ = offset; }

		// PBR material properties
		inline float GetRoughness() const { return roughness_; }

		inline void SetRoughness(float roughness) {
			roughness_ = glm::clamp(roughness, 0.0f, 1.0f);
			MarkDirty();
		}

		inline float GetMetallic() const { return metallic_; }

		inline void SetMetallic(float metallic) {
			metallic_ = glm::clamp(metallic, 0.0f, 1.0f);
			MarkDirty();
		}

		inline float GetAO() const { return ao_; }

		inline void SetAO(float ao) {
			ao_ = glm::clamp(ao, 0.0f, 1.0f);
			MarkDirty();
		}

		inline bool UsePBR() const { return use_pbr_; }

		inline void SetUsePBR(bool use_pbr) {
			use_pbr_ = use_pbr;
			MarkDirty();
		}

		/**
		 * @brief Set the dissolve plane for the shape.
		 * Fragments where dot(FragPos, direction) > dist will be discarded.
		 * dist is usually calculated based on sweep (0.0 to 1.0) and model extent.
		 */
		virtual void SetDissolve(const glm::vec3& direction, float dist) {
			dissolve_plane_normal_ = direction;
			dissolve_plane_dist_ = dist;
			dissolve_enabled_ = true;
			MarkDirty();
		}

		inline void DisableDissolve() {
			dissolve_enabled_ = false;
			MarkDirty();
		}

		inline bool IsDissolveEnabled() const { return dissolve_enabled_; }

		inline const glm::vec3& GetDissolveNormal() const { return dissolve_plane_normal_; }

		inline float GetDissolveDist() const { return dissolve_plane_dist_; }

		// Static shader reference
		static std::shared_ptr<Shader> shader;
		static ShaderHandle            shader_handle;

		// Sphere mesh generation
		static void InitSphereMesh(Megabuffer* megabuffer = nullptr);
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
			int   trail_length = 0,
			float trail_thickness = Constants::Class::Trails::BaseThickness()
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
			local_aabb_(glm::vec3(-1.0f), glm::vec3(1.0f)),
			clamp_to_terrain_(false),
			ground_offset_(0.0f),
			trail_length_(trail_length),
			trail_thickness_(trail_thickness),
			trail_iridescent_(false),
			is_colossal_(false),
			last_position_(x, y, z),
			trail_pbr_(false),
			trail_roughness_(0.3f),
			trail_metallic_(0.0f),
			roughness_(0.5f),
			metallic_(0.0f),
			ao_(1.0f),
			use_pbr_(false),
			dissolve_enabled_(false),
			dissolve_plane_normal_(0, 1, 0),
			dissolve_plane_dist_(0.0f) {}

		glm::quat rotation_;
		glm::vec3 scale_;
		AABB      local_aabb_;
		bool      clamp_to_terrain_;
		float     ground_offset_;

		// Dirty flag pattern for packet caching (thread-safe)
		mutable std::atomic<bool>         render_dirty_{true};
		mutable std::vector<RenderPacket> cached_packets_;

	private:
		int       id_;
		float     x_, y_, z_;
		glm::vec3 last_position_;
		float     r_, g_, b_, a_;
		int       trail_length_;
		float     trail_thickness_;
		bool      trail_iridescent_;
		bool      trail_rocket_;
		bool      is_colossal_;
		bool      is_hidden_ = false;
		bool      trail_pbr_;
		float     trail_roughness_;
		float     trail_metallic_;
		float     roughness_;
		float     metallic_;
		float     ao_;
		bool      use_pbr_;

	protected:
		bool      dissolve_enabled_;
		glm::vec3 dissolve_plane_normal_;
		float     dissolve_plane_dist_;

	public:
		// Shared sphere mesh (public for instancing support)
		static unsigned int         sphere_vao_;
		static unsigned int         sphere_vbo_;
		static unsigned int         sphere_ebo_;
		static int                  sphere_vertex_count_;
		static MegabufferAllocation sphere_alloc_;
	};

	// Function type for user-defined shape generation
	using ShapeFunction = std::function<std::vector<std::shared_ptr<Shape>>(float time)>;
} // namespace Boidsish
