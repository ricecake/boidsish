#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "dot.h"
#include "graphics.h"
#include "logger.h"
#include "path.h"
#include "rigid_body.h"
#include "shape.h"
#include "task_thread_pool.hpp"
#include "vector.h"

namespace Boidsish {

	// Forward declaration for Entity class
	class EntityHandler;
	class TerrainGenerator;

	// Base entity class for the entity system
	class EntityBase {
		friend class EntityHandler;

	public:
		EntityBase(int id = 0):
			id_(id), size_(8.0f), color_{1.0f, 1.0f, 1.0f, 1.0f}, trail_length_(50), trail_iridescent_(false) {}

		virtual ~EntityBase() = default;

		// Called each frame to update the entity
		virtual void UpdateEntity(const EntityHandler& handler, float time, float delta_time) = 0;

		// Shape management
		virtual std::shared_ptr<Shape> GetShape() const = 0;
		virtual void                   UpdateShape() = 0;

		// Getters and setters
		int GetId() const { return id_; }

		// Absolute spatial position
		float GetXPos() const { return rigid_body_.GetPosition().x; }

		float GetYPos() const { return rigid_body_.GetPosition().y; }

		float GetZPos() const { return rigid_body_.GetPosition().z; }

		Vector3 GetPosition() const {
			glm::vec3 pos = rigid_body_.GetPosition();
			return Vector3(pos.x, pos.y, pos.z);
		}

		void SetPosition(float x, float y, float z) { rigid_body_.SetPosition(glm::vec3(x, y, z)); }

		void SetPosition(const Vector3& pos) { rigid_body_.SetPosition(glm::vec3(pos.x, pos.y, pos.z)); }

		// Spatial velocity per frame
		float GetXVel() const { return rigid_body_.GetLinearVelocity().x; }

		float GetYVel() const { return rigid_body_.GetLinearVelocity().y; }

		float GetZVel() const { return rigid_body_.GetLinearVelocity().z; }

		Vector3 GetVelocity() const {
			glm::vec3 vel = rigid_body_.GetLinearVelocity();
			return Vector3(vel.x, vel.y, vel.z);
		}

		void SetVelocity(float vx, float vy, float vz) { rigid_body_.SetLinearVelocity(glm::vec3(vx, vy, vz)); }

		void SetVelocity(const Vector3& vel) { rigid_body_.SetLinearVelocity(glm::vec3(vel.x, vel.y, vel.z)); }

		void SetVelocity(const glm::vec3& vel) { rigid_body_.SetLinearVelocity(vel); }

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

		bool IsTrailIridescent() const { return trail_iridescent_; }

		void SetTrailIridescence(bool enabled) { trail_iridescent_ = enabled; }

		// New method for rocket trail
		void SetTrailRocket(bool enabled) { trail_rocket_ = enabled; }

		void SetOrientToVelocity(bool enabled) { orient_to_velocity_ = enabled; }

		void SetPath(std::shared_ptr<Path> path, float speed) {
			path_ = path;
			path_speed_ = speed;
			path_segment_index_ = 0;
			path_t_ = 0.0f;
		}

		void SetPathConstraint(std::shared_ptr<Path> path, float radius) {
			constraint_path_ = path;
			constraint_radius_ = radius;
		}

		glm::vec3 ObjectToWorld(const glm::vec3& v) const { return rigid_body_.GetOrientation() * v; }

		glm::vec3 WorldToObject(const glm::vec3& v) const { return glm::inverse(rigid_body_.GetOrientation()) * v; }

	protected:
		int       id_;
		RigidBody rigid_body_;
		float     size_;
		float     color_[4]; // RGBA
		int       trail_length_;
		bool      trail_iridescent_;
		bool      trail_rocket_ = false; // New member for rocket trail
		bool      orient_to_velocity_ = false;

		// Path following
		std::shared_ptr<Path> path_;
		float                 path_speed_ = 1.0f;
		int                   path_direction_ = 1;
		int                   path_segment_index_ = 0;
		float                 path_t_ = 0.0f;

		// Path constraint
		std::shared_ptr<Path> constraint_path_;
		float                 constraint_radius_ = 0.0f;
	};

	// Template-based entity class that takes a shape
	template <typename ShapeType = Dot>
	class Entity: public EntityBase {
	public:
		template <typename... ShapeArgs>
		Entity(int id = 0, ShapeArgs... args): EntityBase(id), shape_(nullptr) {
			shape_ = std::make_shared<ShapeType>(std::forward<ShapeArgs>(args)...);
			UpdateShape();
		}

		Entity(int id = 0): EntityBase(id), shape_(nullptr) {
			if constexpr (std::is_default_constructible<ShapeType>::value) {
				shape_ = std::make_shared<ShapeType>();
			}
			UpdateShape();
		}

		std::shared_ptr<Shape> GetShape() const override { return shape_; }

		void SetOrientation(glm::quat orientation) { rigid_body_.SetOrientation(orientation); }

		void UpdateShape() override {
			if (!shape_)
				return;
			shape_->SetId(id_);
			shape_->SetPosition(GetXPos(), GetYPos(), GetZPos());
			shape_->SetColor(color_[0], color_[1], color_[2], color_[3]);
			shape_->SetTrailLength(trail_length_);
			shape_->SetTrailIridescence(trail_iridescent_);
			shape_->SetTrailRocket(trail_rocket_); // Propagate rocket trail state
			shape_->SetRotation(rigid_body_.GetOrientation());
			// For dots, we can also update the size
			if (auto dot = std::dynamic_pointer_cast<Dot>(shape_)) {
				dot->SetSize(size_);
			}
		}

	protected:
		std::shared_ptr<ShapeType> shape_;
	};

	// Entity handler that manages entities and provides dot generation
	class EntityHandler {
	public:
		EntityHandler(
			task_thread_pool::task_thread_pool& thread_pool,
			std::shared_ptr<Visualizer>         visualizer = nullptr
		):
			thread_pool_(thread_pool), vis(visualizer), last_time_(-1.0f), next_id_(0) {}

		virtual ~EntityHandler();

		// Delete copy constructor and assignment operator since we contain shared_ptr
		EntityHandler(const EntityHandler&) = delete;
		EntityHandler& operator=(const EntityHandler&) = delete;

		// Enable move semantics
		EntityHandler(EntityHandler&&) = default;
		EntityHandler& operator=(EntityHandler&&) = default;

		// Operator() to make this compatible with ShapeFunction
		std::vector<std::shared_ptr<Shape>> operator()(float time);

		void SetVisualizer(auto& new_vis) { vis = new_vis; }

		// Entity management
		template <typename T, typename... Args>
		int AddEntity(Args&&... args) {
			int  id = next_id_++;
			auto entity = std::make_shared<T>(id, std::forward<Args>(args)...);
			AddEntity(id, entity);
			return id;
		}

		template <typename T, typename... Args>
		int AddEntity(int id, Args&&... args) {
			// int  id = next_id_++;
			auto entity = std::make_shared<T>(id, std::forward<Args>(args)...);
			AddEntity(id, entity);
			return id;
		}

		virtual void AddEntity(int id, std::shared_ptr<EntityBase> entity) {
			entities_[id] = entity;
			if (vis) {
				entity->UpdateShape();
				vis->AddShape(entity->GetShape());
			}
		}

		virtual void RemoveEntity(int id) {
			if (vis) {
				if (auto entity = GetEntity(id)) {
					if (auto shape = entity->GetShape()) {
						vis->RemoveShape(id);
					}
				}
			}
			entities_.erase(id);
		}

		std::shared_ptr<EntityBase> GetEntity(int id) {
			auto it = entities_.find(id);
			return (it != entities_.end()) ? it->second : nullptr;
		}

		std::shared_ptr<EntityBase> GetEntity(int id) const {
			auto it = entities_.find(id);
			return (it != entities_.end()) ? it->second : nullptr;
		}

		// Get all entities (for iteration)
		const std::map<int, std::shared_ptr<EntityBase>>& GetAllEntities() const { return entities_; }

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
		std::vector<T*> GetEntitiesByType() const {
			std::vector<T*> result;
			for (const auto& pair : entities_) {
				T* typed_entity = dynamic_cast<T*>(pair.second.get());
				if (typed_entity) {
					result.push_back(typed_entity);
				}
			}
			return result;
		}

		// Get total entity count
		size_t GetEntityCount() const { return entities_.size(); }

		std::tuple<float, glm::vec3>                 GetTerrainPointProperties(float x, float y) const;
		std::tuple<float, glm::vec3>                 GetTerrainPointPropertiesThreadSafe(float x, float y) const;
		const std::vector<std::shared_ptr<Terrain>>& GetTerrainChunks() const;
		const TerrainGenerator*                      GetTerrainGenerator() const;

		// Thread-safe methods for entity modification
		template <typename T, typename... Args>
		void QueueAddEntity(Args&&... args) const {
			std::lock_guard<std::mutex> lock(requests_mutex_);
			modification_requests_.emplace_back([this, args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
				std::apply(
					[this](auto&&... a) { const_cast<EntityHandler*>(this)->AddEntity<T>(std::forward<Args>(a)...); },
					std::move(args)
				);
			});
		}

		void QueueRemoveEntity(int id) const {
			std::lock_guard<std::mutex> lock(requests_mutex_);
			modification_requests_.emplace_back([this, id]() { const_cast<EntityHandler*>(this)->RemoveEntity(id); });
		}

		void EnqueueVisualizerAction(std::function<void()> callback) const {
			std::lock_guard<std::mutex> lock(visualizer_mutex_);
			post_frame_requests_.push_back(callback);
		}

		std::shared_ptr<Visualizer> vis;

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

		virtual void OnEntityUpdated(std::shared_ptr<EntityBase> entity) { (void)entity; }

	private:
		std::map<int, std::shared_ptr<EntityBase>> entities_;
		float                                      last_time_;
		int                                        next_id_;
		task_thread_pool::task_thread_pool&        thread_pool_;
		mutable std::vector<std::function<void()>> modification_requests_;
		mutable std::vector<std::function<void()>> post_frame_requests_;
		mutable std::mutex                         requests_mutex_;
		mutable std::mutex                         visualizer_mutex_;
	};
} // namespace Boidsish
