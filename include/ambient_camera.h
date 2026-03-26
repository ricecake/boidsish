#pragma once

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

		void    BuildLUT(int samplesPerSegment = 20);
		Vector3 Evaluate(float u) const; // u in [0, 1] (normalized arc-length)
	};

	/**
	 * @brief Manages procedural, decoupled camera paths for ambient mode.
	 */
	class AmbientCameraSystem {
	public:
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

	private:
		void GenerateNewPaths(ITerrainGenerator* terrain, DecorManager* decor);

		// Path generation helpers
		void GenerateProbePath(ITerrainGenerator* terrain, DecorManager* decor);
		void GenerateCameraPath(ITerrainGenerator* terrain, DecorManager* decor);
		void GenerateFocusPath(ITerrainGenerator* terrain, DecorManager* decor);

		// Splines
		CoordinatedSpline probeSpline;
		CoordinatedSpline cameraSpline;
		CoordinatedSpline focusSpline;

		float globalU = 0.0f;         // Current traversal parameter [0, 1]
		float traversalSpeed = 10.0f; // Units per second
		bool  pathsValid = false;

		// Destination state
		bool      hasDestination = false;
		glm::vec3 targetDestination{0.0f};
		float     pathDirectness = 0.5f;

		// State for path generation
		glm::vec3 lastProbePos{0.0f, 20.0f, 0.0f};
		glm::vec3 lastCameraPos{10.0f, 25.0f, 10.0f};
	};

} // namespace Boidsish
