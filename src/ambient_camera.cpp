#include "ambient_camera.h"

#include <algorithm>

#include "decor_manager.h"
#include "graphics.h"
#include "logger.h"
#include "spline.h"
#include "terrain.h"
#include "terrain_generator_interface.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

	void CoordinatedSpline::BuildLUT(int samplesPerSegment) {
		arcLengthLUT.clear();
		totalLength = 0.0f;

		if (waypoints.size() < 2)
			return;

		int numSegments = static_cast<int>(waypoints.size()) - 1;
		arcLengthLUT.push_back(0.0f);

		for (int i = 0; i < numSegments; ++i) {
			Vector3 p1 = waypoints[i];
			Vector3 p2 = waypoints[i + 1];
			Vector3 p0 = (i == 0) ? p1 - (p2 - p1) : waypoints[i - 1];
			Vector3 p3 = (i == numSegments - 1) ? p2 + (p2 - p1) : waypoints[i + 2];

			Vector3 prevPos = p1;
			for (int j = 1; j <= samplesPerSegment; ++j) {
				float   t = (float)j / (float)samplesPerSegment;
				Vector3 currPos = Spline::CatmullRom(t, p0, p1, p2, p3);
				float   dist = (currPos - prevPos).Magnitude();
				totalLength += dist;
				arcLengthLUT.push_back(totalLength);
				prevPos = currPos;
			}
		}
	}

	Vector3 CoordinatedSpline::Evaluate(float u) const {
		if (waypoints.empty())
			return Vector3(0.0f, 0.0f, 0.0f);
		if (waypoints.size() == 1)
			return waypoints[0];
		if (u <= 0.0f)
			return waypoints.front();
		if (u >= 1.0f)
			return waypoints.back();

		float targetDist = u * totalLength;

		// Binary search for the samples that bracket targetDist
		auto it = std::lower_bound(arcLengthLUT.begin(), arcLengthLUT.end(), targetDist);
		if (it == arcLengthLUT.begin())
			return waypoints.front();

		int   sampleIdx = std::distance(arcLengthLUT.begin(), it) - 1;
		float distA = arcLengthLUT[sampleIdx];
		float distB = arcLengthLUT[sampleIdx + 1];

		// samplesPerSegment was used in BuildLUT
		int samplesPerSegment = (arcLengthLUT.size() - 1) / (waypoints.size() - 1);

		int segmentIdx = sampleIdx / samplesPerSegment;
		int localSampleIdx = sampleIdx % samplesPerSegment;

		float tA = (float)localSampleIdx / (float)samplesPerSegment;
		float tB = (float)(localSampleIdx + 1) / (float)samplesPerSegment;

		// Interpolate t based on distance
		float f = (targetDist - distA) / (distB - distA);
		float t = glm::mix(tA, tB, f);

		// Evaluate Catmull-Rom at segmentIdx with parameter t
		Vector3 p1 = waypoints[segmentIdx];
		Vector3 p2 = waypoints[segmentIdx + 1];
		Vector3 p0 = (segmentIdx == 0) ? p1 - (p2 - p1) : waypoints[segmentIdx - 1];
		int     numSegments = static_cast<int>(waypoints.size()) - 1;
		Vector3 p3 = (segmentIdx == numSegments - 1) ? p2 + (p2 - p1) : waypoints[segmentIdx + 2];

		return Spline::CatmullRom(t, p0, p1, p2, p3);
	}

	AmbientCameraSystem::AmbientCameraSystem() {}

	AmbientCameraSystem::~AmbientCameraSystem() {}

	void AmbientCameraSystem::SetDestination(glm::vec3 dest, float directness) {
		targetDestination = dest;
		pathDirectness = std::clamp(directness, 0.0f, 1.0f);
		hasDestination = true;
	}

	void AmbientCameraSystem::Update(
		float              deltaTime,
		ITerrainGenerator* terrain,
		DecorManager*      decor,
		Camera&            camera,
		glm::vec3&         probePos
	) {
		if (!pathsValid || globalU >= 1.0f) {
			GenerateNewPaths(terrain, decor);
			globalU = 0.0f;
			pathsValid = true;
		}

		// Advance global traversal parameter
		// We want a constant world-space speed.
		// The splines might have different lengths.
		// The Probe spline is the master for speed calculation.
		if (probeSpline.totalLength > 0.001f) {
			globalU += (traversalSpeed * deltaTime) / probeSpline.totalLength;
		} else {
			globalU = 1.0f;
		}

		globalU = std::clamp(globalU, 0.0f, 1.0f);

		// Evaluate all three splines
		Vector3 pPos = probeSpline.Evaluate(globalU);
		Vector3 cPos = cameraSpline.Evaluate(globalU);
		Vector3 fPos = focusSpline.Evaluate(globalU);

		glm::vec3 targetProbePos(pPos.x, pPos.y, pPos.z);
		glm::vec3 targetCameraPos(cPos.x, cPos.y, cPos.z);
		glm::vec3 targetFocusPos(fPos.x, fPos.y, fPos.z);

		if (firstUpdate) {
			smoothedCameraPos = targetCameraPos;
			smoothedFocusPos = targetFocusPos;
			firstUpdate = false;
		}

		// Critically damped spring for camera and focus
		auto criticallyDampedSpring =
			[](glm::vec3& current, glm::vec3& velocity, glm::vec3 target, float omega, float dt) {
				glm::vec3 e = current - target;
				glm::vec3 a = -2.0f * omega * velocity - omega * omega * e;
				velocity += a * dt;
				current += velocity * dt;
			};

		float omega = 3.0f; // Response speed
		criticallyDampedSpring(smoothedCameraPos, cameraVelocity, targetCameraPos, omega, deltaTime);
		criticallyDampedSpring(smoothedFocusPos, focusVelocity, targetFocusPos, omega, deltaTime);

		// Update outputs
		probePos = targetProbePos;

		camera.x = smoothedCameraPos.x;
		camera.y = smoothedCameraPos.y;
		camera.z = smoothedCameraPos.z;

		// Calculate yaw/pitch to look at smoothed focus
		glm::vec3 front = glm::normalize(smoothedFocusPos - smoothedCameraPos);
		camera.yaw = glm::degrees(atan2(front.x, -front.z));
		camera.pitch = glm::degrees(asin(front.y));
		camera.roll = 0.0f;

		// Update state for next generation
		lastProbePos = probePos;
		lastCameraPos = glm::vec3(camera.x, camera.y, camera.z);
	}

	void AmbientCameraSystem::GenerateNewPaths(ITerrainGenerator* terrain, DecorManager* decor) {
		GenerateProbePath(terrain, decor);
		GenerateCameraPath(terrain, decor);
		GenerateFocusPath(terrain, decor);

		probeSpline.BuildLUT();
		cameraSpline.BuildLUT();
		focusSpline.BuildLUT();
	}

	static bool CheckCollision(
		const Vector3&     start,
		const Vector3&     end,
		ITerrainGenerator* terrain,
		DecorManager*      decor,
		float&             hitT
	) {
		glm::vec3 s(start.x, start.y, start.z);
		glm::vec3 e(end.x, end.y, end.z);
		glm::vec3 dir = e - s;
		float     dist = glm::length(dir);
		if (dist < 0.001f)
			return false;
		dir /= dist;

		// 1. Terrain collision
		float terrainDist;
		if (terrain && terrain->Raycast(s, dir, dist, terrainDist)) {
			hitT = terrainDist / dist;
			return true;
		}

		// 2. Decor collision
		if (decor && terrain) {
			// Get decor in chunks around the segment
			std::vector<std::pair<int, int>> chunks;
			float                            worldScale = terrain->GetWorldScale();
			float                            chunkSize = 32.0f * worldScale;

			auto getChunk = [&](const glm::vec3& p) {
				return std::make_pair(
					static_cast<int>(std::floor(p.x / chunkSize)),
					static_cast<int>(std::floor(p.z / chunkSize))
				);
			};

			chunks.push_back(getChunk(s));
			chunks.push_back(getChunk(e));
			// Also midpoint for safety on long segments
			chunks.push_back(getChunk((s + e) * 0.5f));

			// Remove duplicates
			std::sort(chunks.begin(), chunks.end());
			chunks.erase(std::unique(chunks.begin(), chunks.end()), chunks.end());

			auto results = decor->GetDecorInChunks(chunks, terrain->GetRenderManager(), *terrain);

			float minDecorT = 2.0f;
			Ray   ray(s, dir);
			bool  hitDecor = false;

			for (const auto& typeResult : results) {
				// Heuristic: structures are more likely to be big obstacles
				// but for now check everything.
				for (const auto& inst : typeResult.instances) {
					float t;
					if (inst.aabb.Intersects(ray, t)) {
						if (t >= 0.0f && t <= dist) {
							if (t / dist < minDecorT) {
								minDecorT = t / dist;
								hitDecor = true;
							}
						}
					}
				}
			}

			if (hitDecor) {
				hitT = minDecorT;
				return true;
			}
		}

		return false;
	}

	void AmbientCameraSystem::GenerateProbePath(ITerrainGenerator* terrain, DecorManager* decor) {
		probeSpline.waypoints.clear();

		Vector3 current = Vector3(lastProbePos.x, lastProbePos.y, lastProbePos.z);
		probeSpline.waypoints.push_back(current);

		int   numSteps = 8;
		float stepDist = 120.0f;

		for (int i = 0; i < numSteps; ++i) {
			Vector3 next;

			if (hasDestination) {
				glm::vec3 toDest = targetDestination - glm::vec3(current.x, current.y, current.z);
				float     d = glm::length(toDest);
				if (d < 5.0f) {
					hasDestination = false; // Arrived
					float angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
					next = Vector3(current.x + cos(angle) * stepDist, 0, current.z + sin(angle) * stepDist);
				} else {
					glm::vec3 dir = toDest / d;
					// Blend between direct path and random walk
					float     angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
					glm::vec3 wander(cos(angle), 0, sin(angle));
					glm::vec3 finalDir = glm::normalize(glm::mix(wander, dir, pathDirectness));
					next = Vector3(current.x + finalDir.x * stepDist, 0, current.z + finalDir.z * stepDist);
				}
			} else {
				// Random walk: pick a direction
				float angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
				next = Vector3(current.x + cos(angle) * stepDist, 0, current.z + sin(angle) * stepDist);
			}

			// Get terrain height at next
			if (terrain) {
				auto [h, n] = terrain->GetTerrainPropertiesAtPoint(next.x, next.z);
				next.y = h + 10.0f; // Maintain 10 units above ground
			} else {
				next.y = 20.0f;
			}

			// Check collision/LoS and subdivide
			int  subdivisions = 0;
			auto addWaypoints = [&](auto& self, const Vector3& a, const Vector3& b, int depth) -> void {
				if (depth > 3) {
					probeSpline.waypoints.push_back(b);
					return;
				}

				float hitT;
				if (CheckCollision(a, b, terrain, decor, hitT)) {
					// Collision detected. Try to go ABOVE it.
					Vector3 mid = (a + b) * 0.5f;
					mid.y += 15.0f; // Boost height
					self(self, a, mid, depth + 1);
					self(self, mid, b, depth + 1);
				} else {
					probeSpline.waypoints.push_back(b);
				}
			};

			addWaypoints(addWaypoints, current, next, 0);
			current = probeSpline.waypoints.back();
		}
	}

	void AmbientCameraSystem::GenerateCameraPath(ITerrainGenerator* terrain, DecorManager* decor) {
		cameraSpline.waypoints.clear();

		// Start from last camera position
		cameraSpline.waypoints.push_back(Vector3(lastCameraPos.x, lastCameraPos.y, lastCameraPos.z));

		// For each probe waypoint (skipping the first one which is lastProbePos),
		// pick a camera waypoint.
		for (size_t i = 1; i < probeSpline.waypoints.size(); ++i) {
			Vector3 pPos = probeSpline.waypoints[i];

			// Try several random points on a sphere around the probe
			float   baseRadius = 160.0f;                    // Further away to keep probe small on screen
			Vector3 bestCamPos = pPos + Vector3(0, 30, 80); // Default offset
			float   bestScore = -1e10f;

			for (int j = 0; j < 12; ++j) {
				float phi = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
				// Constraints: Avoid directly overhead (theta in [40, 85] degrees)
				float thetaDeg = 40.0f + (float)rand() / RAND_MAX * 45.0f;
				float theta = glm::radians(thetaDeg);

				// Adjust radius based on angle (lower angle = can get closer)
				float angleFactor = (thetaDeg - 40.0f) / 45.0f; // 0 to 1
				float radius = baseRadius * (1.5f - 0.5f * angleFactor);

				Vector3 offset(radius * sin(theta) * cos(phi), radius * cos(theta), radius * sin(theta) * sin(phi));
				Vector3 candidate = pPos + offset;

				// Constraints:
				// 1. Above terrain
				if (terrain) {
					float h = std::get<0>(terrain->GetTerrainPropertiesAtPoint(candidate.x, candidate.z));
					if (candidate.y < h + 5.0f)
						continue;
				}

				// 2. Path Line of Sight (collision avoidance for camera movement)
				float hitT;
				if (CheckCollision(cameraSpline.waypoints.back(), candidate, terrain, decor, hitT)) {
					continue;
				}

				// 3. Line of sight to Probe
				if (CheckCollision(candidate, pPos, terrain, decor, hitT)) {
					if (hitT < 0.95f)
						continue;
				}

				// Score based on distance from previous camera waypoint (prefer smooth)
				float d = (candidate - cameraSpline.waypoints.back()).Magnitude();
				float score = -d;

				if (score > bestScore) {
					bestScore = score;
					bestCamPos = candidate;
				}
			}
			cameraSpline.waypoints.push_back(bestCamPos);
		}
	}

	void AmbientCameraSystem::GenerateFocusPath(ITerrainGenerator* terrain, DecorManager* decor) {
		focusSpline.waypoints.clear();

		// Start from last probe position (what we were looking at)
		focusSpline.waypoints.push_back(Vector3(lastProbePos.x, lastProbePos.y, lastProbePos.z));

		// For each probe waypoint
		for (size_t i = 1; i < probeSpline.waypoints.size(); ++i) {
			Vector3 pPos = probeSpline.waypoints[i];
			Vector3 cPos = cameraSpline.waypoints[i];

			// Default focus near the probe on terrain, not the probe itself
			float   offsetAngle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
			float   offsetDist = 15.0f + (float)rand() / RAND_MAX * 25.0f;
			Vector3 targetFocus = pPos + Vector3(cos(offsetAngle) * offsetDist, 0.0f, sin(offsetAngle) * offsetDist);
			if (terrain) {
				targetFocus.y = std::get<0>(terrain->GetTerrainPropertiesAtPoint(targetFocus.x, targetFocus.z));
			}

			// Check if probe is in a valid focus position (horizon angle constraint)
			glm::vec3 toFocus = glm::normalize(
				glm::vec3(targetFocus.x - cPos.x, targetFocus.y - cPos.y, targetFocus.z - cPos.z)
			);
			float focusAngle = asin(toFocus.y);
			bool  focusValid = (abs(focusAngle) <= 3.14159f / 4.0f);

			// Occasionally look at POIs, or if probe is invalid
			if (!focusValid || rand() % 2 == 0) {
				std::vector<Vector3> pois;

				// 1. Mountain peaks (proxies)
				if (terrain) {
					auto chunks = terrain->GetVisibleChunks();
					for (auto& chunk : chunks) {
						pois.push_back(Vector3(
							chunk->proxy.highestPoint.x,
							chunk->proxy.highestPoint.y,
							chunk->proxy.highestPoint.z
						));
					}
				}

				// 1.5 Sunset/Sun POI
				pois.push_back(cPos + Vector3(0, 10, -500)); // Simple "Sun" direction

				// 2. Decor structures
				if (decor && terrain) {
					float                            worldScale = terrain->GetWorldScale();
					float                            chunkSize = 32.0f * worldScale;
					std::vector<std::pair<int, int>> chunks;
					int                              cx = static_cast<int>(std::floor(cPos.x / chunkSize));
					int                              cz = static_cast<int>(std::floor(cPos.z / chunkSize));
					for (int dx = -2; dx <= 2; ++dx)
						for (int dz = -2; dz <= 2; ++dz)
							chunks.push_back({cx + dx, cz + dz});

					auto results = decor->GetDecorInChunks(chunks, terrain->GetRenderManager(), *terrain);
					for (const auto& tr : results) {
						// Select procedural structures or important trees
						bool isStructure = (tr.model_path.find("structure") != std::string::npos) ||
							(tr.model_path.find("Structure") != std::string::npos);
						bool isTree = (tr.model_path.find("tree") != std::string::npos) ||
							(tr.model_path.find("Tree") != std::string::npos);

						if (isStructure || (isTree && rand() % 5 == 0)) {
							for (const auto& inst : tr.instances) {
								pois.push_back(Vector3(inst.center.x, inst.center.y, inst.center.z));
								if (pois.size() > 20)
									break;
							}
						}
						if (pois.size() > 20)
							break;
					}
				}

				// Pick the best POI
				float bestScore = -1e10f;
				for (const auto& poi : pois) {
					// Check LoS from Camera
					float hitT;
					if (CheckCollision(cPos, poi, terrain, decor, hitT))
						continue;

					// Check horizon angle (max 45 degrees)
					glm::vec3 toPoi = glm::normalize(glm::vec3(poi.x - cPos.x, poi.y - cPos.y, poi.z - cPos.z));
					float     angle = asin(toPoi.y);
					if (abs(angle) > 3.14159f / 4.0f)
						continue;

					// Score based on distance (prefer distant vistas but not too far)
					float d = (poi - cPos).Magnitude();
					float score = -abs(d - 150.0f);

					if (score > bestScore) {
						bestScore = score;
						targetFocus = poi;
					}
				}
			}

			focusSpline.waypoints.push_back(targetFocus);
		}
	}

} // namespace Boidsish
