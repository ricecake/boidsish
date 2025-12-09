#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "shape.h"
#include "vector.h"
#include "dot.h"

namespace Boidsish {

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

	// EntityHandler implementation
	std::vector<std::shared_ptr<Shape>> EntityHandler::operator()(float time) {
		float delta_time = 0.016f; // Default 60 FPS
		if (last_time_ >= 0.0f) {
			delta_time = time - last_time_;
		}
		last_time_ = time;

		// Call pre-timestep hook
		PreTimestep(time, delta_time);

		// Get entities
		std::vector<std::shared_ptr<Entity>> entities;
		std::transform(entities_.begin(), entities_.end(), std::back_inserter(entities), [](const auto& pair) {
			return pair.second;
		});

		// Update all entities
		for (auto& entity : entities) {
			entity->UpdateEntity(*this, time, delta_time);
		}

		// Call post-timestep hook
		PostTimestep(time, delta_time);

		// Generate shapes from entity states
		std::vector<std::shared_ptr<Shape>> shapes;
		shapes.reserve(entities_.size());

		for (auto& entity : entities) {
			// Update entity position using its velocity
			Vector3 new_position = entity->GetPosition() + entity->GetVelocity() * delta_time;
			entity->SetPosition(new_position);

			// Get visual properties
			float r, g, b, a;
			entity->GetColor(r, g, b, a);

			// Create dot at entity's position
			shapes.emplace_back(
				std::make_shared<Dot>(
					entity->GetId(),
					entity->GetXPos(),
					entity->GetYPos(),
					entity->GetZPos(),
					entity->GetSize(),
					r,
					g,
					b,
					a,
					entity->GetTrailLength()
				)
			);
		}

		return shapes;
	}
} // namespace Boidsish