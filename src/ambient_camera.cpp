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

	void CoordinatedSpline::BuildLUT(int samples) {
		samplesPerSegment = samples;
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

	void CoordinatedSpline::AppendWaypoint(Vector3 wp) {
		if (waypoints.empty()) {
			waypoints.push_back(wp);
			arcLengthLUT.push_back(0.0f);
			return;
		}
		waypoints.push_back(wp);
		// LUT rebuild is deferred to the end of the update cycle for efficiency
	}

	void CoordinatedSpline::PopFrontWaypoint() {
		if (waypoints.empty())
			return;
		waypoints.erase(waypoints.begin());
		// LUT rebuild is deferred to the end of the update cycle for efficiency
	}

	Vector3 CoordinatedSpline::Evaluate(float u) const {
		return EvaluateAtDistance(u * totalLength);
	}

	Vector3 CoordinatedSpline::EvaluateAtDistance(float targetDist) const {
		auto [segmentIdx, t] = GetSegmentAndT(targetDist);
		return Evaluate(segmentIdx, t);
	}

	Vector3 CoordinatedSpline::Evaluate(size_t segmentIdx, float t) const {
		if (waypoints.empty())
			return Vector3(0.0f, 0.0f, 0.0f);
		if (waypoints.size() == 1)
			return waypoints[0];
		if (segmentIdx >= waypoints.size() - 1)
			return waypoints.back();

		// Evaluate Catmull-Rom at segmentIdx with parameter t
		Vector3 p1 = waypoints[segmentIdx];
		Vector3 p2 = waypoints[segmentIdx + 1];
		Vector3 p0 = (segmentIdx == 0) ? p1 - (p2 - p1) : waypoints[segmentIdx - 1];
		int     numSegments = static_cast<int>(waypoints.size()) - 1;
		Vector3 p3 = (segmentIdx == numSegments - 1) ? p2 + (p2 - p1) : waypoints[segmentIdx + 2];

		return Spline::CatmullRom(t, p0, p1, p2, p3);
	}

	std::pair<size_t, float> CoordinatedSpline::GetSegmentAndT(float targetDist) const {
		if (waypoints.size() < 2)
			return {0, 0.0f};
		if (targetDist <= 0.0f)
			return {0, 0.0f};
		if (targetDist >= totalLength)
			return {GetSegmentCount() - 1, 1.0f};

		// Binary search for the samples that bracket targetDist
		auto it = std::lower_bound(arcLengthLUT.begin(), arcLengthLUT.end(), targetDist);
		if (it == arcLengthLUT.begin())
			return {0, 0.0f};

		int   sampleIdx = std::distance(arcLengthLUT.begin(), it) - 1;
		float distA = arcLengthLUT[sampleIdx];
		float distB = arcLengthLUT[sampleIdx + 1];

		size_t segmentIdx = sampleIdx / samplesPerSegment;
		int    localSampleIdx = sampleIdx % samplesPerSegment;

		float tA = (float)localSampleIdx / (float)samplesPerSegment;
		float tB = (float)(localSampleIdx + 1) / (float)samplesPerSegment;

		// Interpolate t based on distance
		float f = (targetDist - distA) / (distB - distA);
		float t = glm::mix(tA, tB, f);

		return {segmentIdx, t};
	}

	float CoordinatedSpline::GetSegmentLength(size_t segmentIdx) const {
		if (segmentIdx >= waypoints.size() - 1)
			return 0.0f;
		return arcLengthLUT[(segmentIdx + 1) * samplesPerSegment] - arcLengthLUT[segmentIdx * samplesPerSegment];
	}

	AmbientCameraSystem::AmbientCameraSystem() {
		m_nextDestinationCallback = [this](ITerrainGenerator* terrain) { return GetDefaultNextDestination(terrain); };
	}

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
		if (!pathsValid) {
			GenerateNewPaths(terrain, decor);
			pathsValid = true;
		}

		// Advance traversal distance along probeSpline
		traversalDistance += traversalSpeed * deltaTime;

		// Sliding window management for smoothness:
		// To evaluate segment 1 (p1 to p2) with full C1 continuity, we need p0, p1, p2, p3.
		// We keep the active segment at index 1 so that index 0 is always a real waypoint,
		// avoiding synthesized virtual points in Catmull-Rom.
		bool modified = false;
		while (probeSpline.waypoints.size() > 5 &&
		       traversalDistance > probeSpline.GetSegmentLength(0) + probeSpline.GetSegmentLength(1)) {
			float len = probeSpline.GetSegmentLength(0);
			traversalDistance -= len;

			probeSpline.PopFrontWaypoint();
			cameraSpline.PopFrontWaypoint();
			focusSpline.PopFrontWaypoint();
			modified = true;
		}

		// Top up waypoints
		while (probeSpline.waypoints.size() < 12) {
			AddWaypointToAllSplines(terrain, decor);
			modified = true;
		}

		if (modified) {
			probeSpline.BuildLUT();
			cameraSpline.BuildLUT();
			focusSpline.BuildLUT();
		}

		// Coordination: find segment index and local t based on probeSpline
		auto [segmentIdx, t] = probeSpline.GetSegmentAndT(traversalDistance);

		// Evaluate all three splines using the SAME segment index and local t for perfect synchronization
		Vector3 pPos = probeSpline.Evaluate(segmentIdx, t);
		Vector3 cPos = cameraSpline.Evaluate(segmentIdx, t);
		Vector3 fPos = focusSpline.Evaluate(segmentIdx, t);

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
		probeSpline.waypoints.clear();
		cameraSpline.waypoints.clear();
		focusSpline.waypoints.clear();

		Vector3 p0 = Vector3(lastProbePos.x, lastProbePos.y, lastProbePos.z);
		Vector3 c0 = Vector3(lastCameraPos.x, lastCameraPos.y, lastCameraPos.z);
		Vector3 f0 = Vector3(lastProbePos.x, lastProbePos.y, lastProbePos.z);

		probeSpline.waypoints.push_back(p0);
		cameraSpline.waypoints.push_back(c0);
		focusSpline.waypoints.push_back(f0);

		// Build initial set of waypoints
		for (int i = 0; i < 12; ++i) {
			AddWaypointToAllSplines(terrain, decor);
		}

		probeSpline.BuildLUT();
		cameraSpline.BuildLUT();
		focusSpline.BuildLUT();

		traversalDistance = 0.0f;
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

	void AmbientCameraSystem::AddWaypointToAllSplines(ITerrainGenerator* terrain, DecorManager* decor) {
		// 1. Generate Probe Waypoint
		Vector3   current = probeSpline.waypoints.back();
		float     stepDist = 120.0f;
		glm::vec3 current_g(current.x, current.y, current.z);

		if (!hasDestination && m_nextDestinationCallback) {
			targetDestination = m_nextDestinationCallback(terrain);
			hasDestination = true;
		}

		Vector3 next;
		if (hasDestination) {
			glm::vec3 toDest = targetDestination - current_g;
			float     d = glm::length(toDest);
			if (d < 10.0f) {
				hasDestination = false; // Arrived
				// Pick a direction for the intermediate step while we wait for next destination
				float angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
				next = Vector3(current.x + cos(angle) * stepDist, 0, current.z + sin(angle) * stepDist);
			} else {
				glm::vec3 dir = toDest / d;
				float     angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
				glm::vec3 wander(cos(angle), 0, sin(angle));
				glm::vec3 finalDir = glm::normalize(glm::mix(wander, dir, pathDirectness));
				next = Vector3(current.x + finalDir.x * stepDist, 0, current.z + finalDir.z * stepDist);
			}
		} else {
			float angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
			next = Vector3(current.x + cos(angle) * stepDist, 0, current.z + sin(angle) * stepDist);
		}

		if (terrain) {
			auto [h, n] = terrain->GetTerrainPropertiesAtPoint(next.x, next.z);
			next.y = h + 10.0f;
		} else {
			next.y = 20.0f;
		}

		// Subdivision / Collision for Probe
		std::vector<Vector3> tempProbeWaypoints;
		auto                 addWaypoints = [&](auto& self, const Vector3& a, const Vector3& b, int depth) -> void {
			if (depth > 2) {
				tempProbeWaypoints.push_back(b);
				return;
			}
			float hitT;
			if (CheckCollision(a, b, terrain, decor, hitT)) {
				Vector3 mid = (a + b) * 0.5f;
				mid.y += 15.0f;
				self(self, a, mid, depth + 1);
				self(self, mid, b, depth + 1);
			} else {
				tempProbeWaypoints.push_back(b);
			}
		};
		addWaypoints(addWaypoints, current, next, 0);

		// Now we have one or more probe points. For each, add a Camera and Focus point.
		for (const auto& pPos : tempProbeWaypoints) {
			probeSpline.AppendWaypoint(pPos);

			// 2. Generate Camera Waypoint
			Vector3 lastCam = cameraSpline.waypoints.back();
			float   baseRadius = 160.0f;
			Vector3 bestCamPos = pPos + Vector3(0, 30, 80);
			float   bestScore = -1e10f;

			for (int j = 0; j < 12; ++j) {
				float phi = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
				float thetaDeg = 40.0f + (float)rand() / RAND_MAX * 45.0f;
				float theta = glm::radians(thetaDeg);
				float angleFactor = (thetaDeg - 40.0f) / 45.0f;
				float radius = baseRadius * (1.5f - 0.5f * angleFactor);

				Vector3 offset(radius * sin(theta) * cos(phi), radius * cos(theta), radius * sin(theta) * sin(phi));
				Vector3 candidate = pPos + offset;

				if (terrain) {
					float h = std::get<0>(terrain->GetTerrainPropertiesAtPoint(candidate.x, candidate.z));
					if (candidate.y < h + 5.0f)
						continue;
				}

				float hitT;
				if (CheckCollision(lastCam, candidate, terrain, decor, hitT))
					continue;
				if (CheckCollision(candidate, pPos, terrain, decor, hitT)) {
					if (hitT < 0.95f)
						continue;
				}

				float d = (candidate - lastCam).Magnitude();
				float score = -d;
				if (score > bestScore) {
					bestScore = score;
					bestCamPos = candidate;
				}
			}
			cameraSpline.AppendWaypoint(bestCamPos);
			lastCam = bestCamPos;

			// 3. Generate Focus Waypoint
			Vector3 lastFocus = focusSpline.waypoints.back();
			Vector3 cPos = bestCamPos;

			float   offsetAngle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
			float   offsetDist = 15.0f + (float)rand() / RAND_MAX * 25.0f;
			Vector3 targetFocus = pPos + Vector3(cos(offsetAngle) * offsetDist, 0.0f, sin(offsetAngle) * offsetDist);
			if (terrain) {
				targetFocus.y = std::get<0>(terrain->GetTerrainPropertiesAtPoint(targetFocus.x, targetFocus.z));
			}

			glm::vec3 toFocus = glm::normalize(
				glm::vec3(targetFocus.x - cPos.x, targetFocus.y - cPos.y, targetFocus.z - cPos.z)
			);
			float focusAngle = asin(toFocus.y);
			bool  focusValid = (std::abs(focusAngle) <= 3.14159f / 4.0f);

			if (!focusValid || rand() % 2 == 0) {
				std::vector<Vector3> pois;
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
				pois.push_back(cPos + Vector3(0, 10, -500));

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
						bool isStructure = (tr.model_path.find("structure") != std::string::npos) ||
							(tr.model_path.find("Structure") != std::string::npos);
						if (isStructure) {
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

				float bestScorePOI = -1e10f;
				for (const auto& poi : pois) {
					float hitT;
					if (CheckCollision(cPos, poi, terrain, decor, hitT))
						continue;
					glm::vec3 toPoi = glm::normalize(glm::vec3(poi.x - cPos.x, poi.y - cPos.y, poi.z - cPos.z));
					float     angle = asin(toPoi.y);
					if (std::abs(angle) > 3.14159f / 4.0f)
						continue;
					float d = (poi - cPos).Magnitude();
					float score = -std::abs(d - 150.0f);
					if (score > bestScorePOI) {
						bestScorePOI = score;
						targetFocus = poi;
					}
				}
			}
			focusSpline.AppendWaypoint(targetFocus);
		}
	}

	glm::vec3 AmbientCameraSystem::GetDefaultNextDestination(ITerrainGenerator* terrain) {
		if (!terrain)
			return glm::vec3(0, 0, 0);

		// Tour of biomes
		Biome targetBiome = static_cast<Biome>(m_currentBiomeTourIndex);
		m_currentBiomeTourIndex = (m_currentBiomeTourIndex + 1) % static_cast<int>(Biome::Count);

		// Find a target control value for this biome using its weight
		float totalWeight = 0.0f;
		for (const auto& b : kBiomes) {
			totalWeight += b.weight;
		}

		float accumulatedWeight = 0.0f;
		for (uint32_t i = 0; i < static_cast<uint32_t>(targetBiome); ++i) {
			accumulatedWeight += kBiomes[i].weight;
		}
		float targetControl = (accumulatedWeight + kBiomes[static_cast<uint32_t>(targetBiome)].weight * 0.5f) /
			totalWeight;

		glm::vec3 currentProbePos(probeSpline.waypoints.back().x, 0, probeSpline.waypoints.back().z);
		glm::vec3 bestDest = currentProbePos;
		float     bestScore = -1.0f;

		for (int i = 0; i < 40; ++i) {
			float     radius = 800.0f + (float)rand() / RAND_MAX * 1600.0f;
			float     angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
			glm::vec3 candidate = currentProbePos + glm::vec3(cos(angle) * radius, 0, sin(angle) * radius);

			float control = terrain->GetBiomeControlValue(candidate.x, candidate.z);
			float score = 1.0f - std::abs(control - targetControl);

			if (score > bestScore) {
				bestScore = score;
				bestDest = candidate;
			}
		}

		return bestDest;
	}

} // namespace Boidsish
