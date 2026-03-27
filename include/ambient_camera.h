#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "vector.h"
#include <glm/glm.hpp>

namespace Boidsish {

	class ITerrainGenerator;
	class DecorManager;
	struct Camera;

	/**
	 * @brief A spline that supports arc-length parameterization for constant speed traversal.
	 */
	struct CoordinatedSpline {
		std::vector<Vector3> waypoints;
		std::vector<float>   arcLengthLUT; // Maps index/t to cumulative distance
		float                totalLength = 0.0f;
		int                  samplesPerSegment = 20;

		void    BuildLUT(int samplesPerSegment = 20);
		void    AppendWaypoint(Vector3 wp);
		void    PopFrontWaypoint();
		Vector3 Evaluate(float u) const;                    // u in [0, 1] (normalized arc-length)
		Vector3 EvaluateAtDistance(float dist) const;       // Absolute distance
		Vector3 Evaluate(size_t segmentIdx, float t) const; // t in [0, 1] within segment
		float   GetSegmentLength(size_t segmentIdx) const;

		size_t GetSegmentCount() const { return waypoints.size() < 2 ? 0 : waypoints.size() - 1; }

		// Helper to find segment index and local t from absolute distance
		std::pair<size_t, float> GetSegmentAndT(float dist) const;
	};

	/**
	 * @brief Manages procedural, decoupled camera paths for ambient mode.
	 */
	class AmbientCameraSystem {
	public:
		using NextDestinationCallback = std::function<glm::vec3(ITerrainGenerator*)>;

		AmbientCameraSystem();
		~AmbientCameraSystem();

		/**
		 * @brief Updates the system and drives the camera.
		 * @param deltaTime Time since last frame
		 * @param terrain Terrain generator for collision/POI queries
		 * @param decor Decor manager for collision queries
		 * @param camera [out] The camera state to update
		 * @param probePos [out] Current position of the probe (for visualization)
		 */
		void
		Update(float deltaTime, ITerrainGenerator* terrain, DecorManager* decor, Camera& camera, glm::vec3& probePos);

		/**
		 * @brief Sets a target destination for the probe.
		 * @param dest The target world position
		 * @param directness [0, 1] where 1 is a straight line and 0 is high wander.
		 */
		void SetDestination(glm::vec3 dest, float directness = 0.5f);

		/**
		 * @brief Sets a callback to be called when the system needs a new destination.
		 */
		void SetNextDestinationCallback(NextDestinationCallback cb) { m_nextDestinationCallback = cb; }

		/**
		 * @brief Check if the probe has reached its current destination.
		 */
		bool HasReachedDestination() const { return !hasDestination; }

		// Getters for visualization
		const CoordinatedSpline& GetProbeSpline() const { return probeSpline; }

		const CoordinatedSpline& GetCameraSpline() const { return cameraSpline; }

		const CoordinatedSpline& GetFocusSpline() const { return focusSpline; }

		float GetTraversalDistance() const { return traversalDistance; }

	private:
		void GenerateNewPaths(ITerrainGenerator* terrain, DecorManager* decor);

		// Incremental path generation
		void AddWaypointToAllSplines(ITerrainGenerator* terrain, DecorManager* decor);

		// Default destination logic
		glm::vec3 GetDefaultNextDestination(ITerrainGenerator* terrain);

		// Splines
		CoordinatedSpline probeSpline;
		CoordinatedSpline cameraSpline;
		CoordinatedSpline focusSpline;

		float traversalDistance = 0.0f; // Absolute distance along probeSpline
		float traversalSpeed = 10.0f;   // Units per second
		bool  pathsValid = false;

		// Destination state
		bool                    hasDestination = false;
		glm::vec3               targetDestination{0.0f};
		float                   pathDirectness = 0.5f;
		NextDestinationCallback m_nextDestinationCallback;
		int                     m_currentBiomeTourIndex = 0;

		// State for path generation
		glm::vec3 lastProbePos{0.0f, 20.0f, 0.0f};
		glm::vec3 lastCameraPos{10.0f, 25.0f, 10.0f};

		// Smoothing state
		glm::vec3 smoothedCameraPos{0.0f};
		glm::vec3 cameraVelocity{0.0f};
		glm::vec3 smoothedFocusPos{0.0f};
		glm::vec3 focusVelocity{0.0f};
		bool      firstUpdate = true;
	};

} // namespace Boidsish
