#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace Boidsish {

	// Generic 3D vector class with basic operations
	class Vector3 {
	public:
		float x, y, z;

		// Constructors
		constexpr Vector3(): x(0.0f), y(0.0f), z(0.0f) {}

		constexpr Vector3(float x, float y, float z): x(x), y(y), z(z) {}

		// Copy constructor and assignment
		Vector3(const Vector3& other) = default;
		Vector3& operator=(const Vector3& other) = default;

		// Vector addition
		constexpr Vector3 operator+(const Vector3& other) const {
			return Vector3(x + other.x, y + other.y, z + other.z);
		}

		Vector3& operator+=(const Vector3& other) {
			x += other.x;
			y += other.y;
			z += other.z;
			return *this;
		}

		// Vector subtraction
		constexpr Vector3 operator-(const Vector3& other) const {
			return Vector3(x - other.x, y - other.y, z - other.z);
		}

		Vector3& operator-=(const Vector3& other) {
			x -= other.x;
			y -= other.y;
			z -= other.z;
			return *this;
		}

		// Scalar multiplication
		constexpr Vector3 operator*(float scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }

		Vector3& operator*=(float scalar) {
			x *= scalar;
			y *= scalar;
			z *= scalar;
			return *this;
		}

		// Scalar division
		constexpr Vector3 operator/(float scalar) const {
			float inv = 1.0f / scalar;
			return Vector3(x * inv, y * inv, z * inv);
		}

		Vector3& operator/=(float scalar) {
			float inv = 1.0f / scalar;
			x *= inv;
			y *= inv;
			z *= inv;
			return *this;
		}

		// Unary minus
		constexpr Vector3 operator-() const { return Vector3(-x, -y, -z); } // Magnitude (length)

		float Magnitude() const { return std::sqrt(x * x + y * y + z * z); }

		// Squared magnitude (for efficiency when comparing lengths)
		constexpr float MagnitudeSquared() const { return x * x + y * y + z * z; }

		// Normalize to unit length
		Vector3 Normalized() const {
			float mag = Magnitude();
			if (mag > 0.0f) {
				return *this / mag;
			}
			return Vector3(0, 0, 0);
		}

		// Normalize in place
		void Normalize() {
			float mag = Magnitude();
			if (mag > 0.0f) {
				*this /= mag;
			}
		}

		// Dot product
		constexpr float Dot(const Vector3& other) const { return x * other.x + y * other.y + z * other.z; }

		// Cross product
		constexpr Vector3 Cross(const Vector3& other) const {
			return Vector3(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
		} // Spherical angle difference (angle between two vectors in radians)

		float AngleTo(const Vector3& other) const {
			float dot_product = Dot(other);
			float magnitudes = Magnitude() * other.Magnitude();
			if (magnitudes > 0.0f) {
				// Clamp to avoid floating point errors in acos
				float cos_angle = std::max(-1.0f, std::min(1.0f, dot_product / magnitudes));
				return std::acos(cos_angle);
			}
			return 0.0f;
		}

		// Distance to another vector
		float DistanceTo(const Vector3& other) const { return (*this - other).Magnitude(); }

		// Set components
		constexpr void Set(float x, float y, float z) {
			this->x = x;
			this->y = y;
			this->z = z;
		}

		// Zero vector
		static constexpr Vector3 Zero() { return Vector3(0, 0, 0); }

		static constexpr Vector3 One() { return Vector3(1, 1, 1); }

		static constexpr Vector3 Up() { return Vector3(0, 1, 0); }

		static constexpr Vector3 Right() { return Vector3(1, 0, 0); }

		static constexpr Vector3 Forward() { return Vector3(0, 0, 1); }
	};

	// Scalar multiplication (scalar * vector)
	constexpr inline Vector3 operator*(float scalar, const Vector3& vec) {
		return vec * scalar;
	}

	// Base class for all renderable shapes
	class Shape {
	public:
		virtual ~Shape() = default;

		// Common properties
		int   id;
		float x, y, z;
		float r, g, b, a;
		int   trail_length;
	};

	// Structure representing a single dot/particle

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

		void render() const;
	};

	// Function type for user-defined shape generation
	using ShapeFunction = std::function<std::vector<std::shared_ptr<Shape>>(float time)>;

	// Forward declaration for Entity class
	class EntityHandler;

	// Base entity class for the entity system
	class Entity {
	public:
		Entity(int id = 0):
			id_(id),
			position_(0.0f, 0.0f, 0.0f),
			velocity_(0.0f, 0.0f, 0.0f),
			size_(8.0f),
			color_{1.0f, 1.0f, 1.0f, 1.0f},
			trail_length_(50) {}

		virtual ~Entity() = default;

		// Called each frame to update the entity
		virtual void UpdateEntity(EntityHandler& handler, float time, float delta_time) = 0;

		// Getters and setters
		int GetId() const { return id_; }

		// Absolute spatial position
		float GetXPos() const { return position_.x; }

		float GetYPos() const { return position_.y; }

		float GetZPos() const { return position_.z; }

		const Vector3& GetPosition() const { return position_; }

		void SetPosition(float x, float y, float z) { position_.Set(x, y, z); }

		void SetPosition(const Vector3& pos) { position_ = pos; }

		// Spatial velocity per frame
		float GetXVel() const { return velocity_.x; }

		float GetYVel() const { return velocity_.y; }

		float GetZVel() const { return velocity_.z; }

		const Vector3& GetVelocity() const { return velocity_; }

		void SetVelocity(float vx, float vy, float vz) { velocity_.Set(vx, vy, vz); }

		void SetVelocity(const Vector3& vel) { velocity_ = vel; }

		// Visual properties
		float GetSize() const { return size_; }

		void SetSize(float size) { size_ = size; }

		void GetColor(float& r, float& g, float& b, float& a) const {
			r = color_[0];
			g = color_[1];
			b = color_[2];
			a = color_[3];
		}

		void SetColor(float r, float g, float b, float a = 1.0f) {
			color_[0] = r;
			color_[1] = g;
			color_[2] = b;
			color_[3] = a;
		}

		int GetTrailLength() const { return trail_length_; }

		void SetTrailLength(int length) { trail_length_ = length; }

	protected:
		int     id_;
		Vector3 position_; // Absolute spatial position
		Vector3 velocity_; // Spatial velocity per frame
		float   size_;
		float   color_[4]; // RGBA
		int     trail_length_;
	};

	// Entity handler that manages entities and provides dot generation
	class EntityHandler {
	public:
		EntityHandler(): last_time_(-1.0f), next_id_(0) {}

		virtual ~EntityHandler() = default;

		// Delete copy constructor and assignment operator since we contain shared_ptr
		EntityHandler(const EntityHandler&) = delete;
		EntityHandler& operator=(const EntityHandler&) = delete;

		// Enable move semantics
		EntityHandler(EntityHandler&&) = default;
		EntityHandler& operator=(EntityHandler&&) = default;

		// Operator() to make this compatible with ShapeFunction
		std::vector<std::shared_ptr<Shape>> operator()(float time);

		// Entity management
		template <typename T, typename... Args>
		int AddEntity(Args&&... args) {
			int  id = next_id_++;
			auto entity = std::make_shared<T>(id, std::forward<Args>(args)...);
			AddEntity(id, entity);
			return id;
		}

		virtual void AddEntity(int id, std::shared_ptr<Entity> entity) { entities_[id] = entity; }

		virtual void RemoveEntity(int id) { entities_.erase(id); }

		auto GetEntity(int id) {
			auto it = entities_.find(id);
			return (it != entities_.end()) ? it->second : nullptr;
		}

		const auto GetEntity(int id) const {
			auto it = entities_.find(id);
			return (it != entities_.end()) ? it->second : nullptr;
		}

		// Get all entities (for iteration)
		const std::map<int, std::shared_ptr<Entity>>& GetAllEntities() const { return entities_; }

		// Get entities by type (template method)
		template <typename T>
		auto GetEntitiesByType() {
			std::vector<std::shared_ptr<T>> result;
			for (auto& pair : entities_) {
				auto typed_entity = std::dynamic_pointer_cast<T>(pair.second);
				if (typed_entity) {
					result.push_back(typed_entity);
				}
			}
			return result;
		}

		template <typename T>
		std::vector<const T*> GetEntitiesByType() const {
			std::vector<const T*> result;
			for (const auto& pair : entities_) {
				const T* typed_entity = dynamic_cast<const T*>(pair.second.get());
				if (typed_entity) {
					result.push_back(typed_entity);
				}
			}
			return result;
		}

		// Get total entity count
		size_t GetEntityCount() const { return entities_.size(); }

	protected:
		// Override these for custom behavior
		virtual void PreTimestep(float time, float delta_time) {
			(void)time;
			(void)delta_time;
		}

		virtual void PostTimestep(float time, float delta_time) {
			(void)time;
			(void)delta_time;
		}

	private:
		std::map<int, std::shared_ptr<Entity>> entities_;
		float                                  last_time_;
		int                                    next_id_;
	};

	// Camera structure for 3D view control
	struct Camera {
		float x, y, z;    // Camera position
		float pitch, yaw; // Camera rotation
		float fov;        // Field of view

		constexpr Camera(
			float x = 0.0f,
			float y = 0.0f,
			float z = 5.0f,
			float pitch = 0.0f,
			float yaw = 0.0f,
			float fov = 45.0f
		):
			x(x), y(y), z(z), pitch(pitch), yaw(yaw), fov(fov) {}
	};

	// Main visualization class
	class Visualizer {
	public:
		Visualizer(int width = 800, int height = 600, const char* title = "Boidsish 3D Visualizer");
		~Visualizer();

		// Set the function/handler that generates shapes for each frame
		void SetShapeHandler(ShapeFunction func);

		// Legacy method name for compatibility
		void SetDotFunction(ShapeFunction func) { SetShapeHandler(func); }

		// Start the visualization loop
		void Run();

		// Check if the window should close
		bool ShouldClose() const;

		// Update one frame
		void Update();

		// Render one frame
		void Render();

		// Get current camera
		Camera& GetCamera();

		// Set camera position and orientation
		void SetCamera(const Camera& camera);

	private:
		struct VisualizerImpl;
		VisualizerImpl* impl;
	};

} // namespace Boidsish