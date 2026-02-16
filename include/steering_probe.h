#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace Boidsish {

	class ITerrainGenerator;
	class EntityHandler;
	class EntityBase;

	class SteeringProbe {
	public:
		SteeringProbe(std::shared_ptr<ITerrainGenerator> terrain = nullptr);

		void Update(float dt, const glm::vec3& playerPos, const glm::vec3& playerVel);

		void HandleCheckpoints(float dt, EntityHandler& handler, std::shared_ptr<EntityBase> player);

		// Configuration Setters
		void SetMass(float m) { mass_ = m; }

		void SetDrag(float d) { drag_ = d; }

		void SetSpringStiffness(float s) { springStiffness_ = s; }

		void SetValleySlideStrength(float v) { valleySlideStrength_ = v; }

		void SetFlyHeight(float h) { flyHeight_ = h; }

		void SetNorthBiasStrength(float n) { northBiasStrength_ = n; }

		void SetAvoidanceLookAhead(float a) { avoidanceLookAhead_ = a; }

		void SetAvoidanceRadius(float r) { avoidanceRadius_ = r; }

		void SetAvoidanceStrength(float s) { avoidanceStrength_ = s; }

		// Getters
		glm::vec3 GetPosition() const { return position_; }

		glm::vec3 GetVelocity() const { return velocity_; }

		void SetPosition(const glm::vec3& p) { position_ = p; }

		void SetVelocity(const glm::vec3& v) { velocity_ = v; }

		void SetTerrain(std::shared_ptr<ITerrainGenerator> t) { terrain_ = t; }

	private:
		glm::vec3                          position_{0.0f};
		glm::vec3                          velocity_{0.0f};
		std::shared_ptr<ITerrainGenerator> terrain_;

		// Physics parameters
		float mass_ = 2.00f;
		float drag_ = 0.95f;            // Air resistance
		float springStiffness_ = 0.50f; // How hard the leash pulls
		float valleySlideStrength_ = 60.0f;
		float flyHeight_ = 30.0f;
		float northBiasStrength_ = 15.0f;

		// Avoidance parameters
		float avoidanceLookAhead_ = 60.0f;
		float avoidanceRadius_ = 25.0f;
		float avoidanceStrength_ = 20.0f;

		// State for dropping checkpoints
		glm::vec3        lastCheckpointPos_{0.0f};
		glm::vec3        lastCheckpointDir_{0.0f, 0.0f, -1.0f};
		float            timeSinceLastDrop_ = 0.0f;
		std::vector<int> activeCheckpoints_;
	};

} // namespace Boidsish
