#include "ambient_camera.h"
#include "spline.h"
#include "graphics.h"
#include "terrain_generator_interface.h"
#include "terrain.h"
#include "decor_manager.h"
#include "logger.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

    void CoordinatedSpline::BuildLUT(int samplesPerSegment) {
        arcLengthLUT.clear();
        totalLength = 0.0f;

        if (waypoints.size() < 2) return;

        int numSegments = static_cast<int>(waypoints.size()) - 1;
        arcLengthLUT.push_back(0.0f);

        for (int i = 0; i < numSegments; ++i) {
            Vector3 p1 = waypoints[i];
            Vector3 p2 = waypoints[i + 1];
            Vector3 p0 = (i == 0) ? p1 - (p2 - p1) : waypoints[i - 1];
            Vector3 p3 = (i == numSegments - 1) ? p2 + (p2 - p1) : waypoints[i + 2];

            Vector3 prevPos = p1;
            for (int j = 1; j <= samplesPerSegment; ++j) {
                float t = (float)j / (float)samplesPerSegment;
                Vector3 currPos = Spline::CatmullRom(t, p0, p1, p2, p3);
                float dist = (currPos - prevPos).Magnitude();
                totalLength += dist;
                arcLengthLUT.push_back(totalLength);
                prevPos = currPos;
            }
        }
    }

    Vector3 CoordinatedSpline::Evaluate(float u) const {
        if (waypoints.empty()) return Vector3(0.0f, 0.0f, 0.0f);
        if (waypoints.size() == 1) return waypoints[0];
        if (u <= 0.0f) return waypoints.front();
        if (u >= 1.0f) return waypoints.back();

        float targetDist = u * totalLength;

        // Binary search for the samples that bracket targetDist
        auto it = std::lower_bound(arcLengthLUT.begin(), arcLengthLUT.end(), targetDist);
        if (it == arcLengthLUT.begin()) return waypoints.front();

        int sampleIdx = std::distance(arcLengthLUT.begin(), it) - 1;
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
        int numSegments = static_cast<int>(waypoints.size()) - 1;
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

    void AmbientCameraSystem::Update(float deltaTime,
                                     ITerrainGenerator* terrain,
                                     DecorManager* decor,
                                     Camera& camera,
                                     glm::vec3& probePos) {
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

        // Update outputs
        probePos = glm::vec3(pPos.x, pPos.y, pPos.z);

        camera.x = cPos.x;
        camera.y = cPos.y;
        camera.z = cPos.z;

        // Calculate yaw/pitch to look at focus
        glm::vec3 front = glm::normalize(glm::vec3(fPos.x - cPos.x, fPos.y - cPos.y, fPos.z - cPos.z));
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

    static bool CheckCollision(const Vector3& start, const Vector3& end, ITerrainGenerator* terrain, DecorManager* decor, float& hitT) {
        glm::vec3 s(start.x, start.y, start.z);
        glm::vec3 e(end.x, end.y, end.z);
        glm::vec3 dir = e - s;
        float dist = glm::length(dir);
        if (dist < 0.001f) return false;
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
            float worldScale = terrain->GetWorldScale();
            float chunkSize = 32.0f * worldScale;

            auto getChunk = [&](const glm::vec3& p) {
                return std::make_pair(static_cast<int>(std::floor(p.x / chunkSize)), static_cast<int>(std::floor(p.z / chunkSize)));
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
            Ray ray(s, dir);
            bool hitDecor = false;

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

        int numSteps = 6;
        float stepDist = 40.0f;

        for (int i = 0; i < numSteps; ++i) {
            Vector3 next;

            if (hasDestination) {
                glm::vec3 toDest = targetDestination - glm::vec3(current.x, current.y, current.z);
                float d = glm::length(toDest);
                if (d < 5.0f) {
                    hasDestination = false; // Arrived
                    float angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
                    next = Vector3(current.x + cos(angle) * stepDist, 0, current.z + sin(angle) * stepDist);
                } else {
                    glm::vec3 dir = toDest / d;
                    // Blend between direct path and random walk
                    float angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
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
            int subdivisions = 0;
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
            float baseRadius = 40.0f; // Increased to ensure probe isn't too large
            Vector3 bestCamPos = pPos + Vector3(0, 15, 30); // Default offset
            float bestScore = -1e10f;

            for (int j = 0; j < 12; ++j) {
                float phi = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
                // Constraints: Avoid directly overhead (theta in [15, 80] degrees)
                float thetaDeg = 15.0f + (float)rand() / RAND_MAX * 65.0f;
                float theta = glm::radians(thetaDeg);

                // Adjust radius based on angle (lower angle = can get closer)
                // If thetaDeg is small (near zenith), we stay far.
                // If thetaDeg is large (near horizon), we can get closer.
                float angleFactor = (thetaDeg - 15.0f) / 65.0f; // 0 to 1
                float radius = baseRadius * (1.5f - 0.5f * angleFactor);

                Vector3 offset(
                    radius * sin(theta) * cos(phi),
                    radius * cos(theta),
                    radius * sin(theta) * sin(phi)
                );
                Vector3 candidate = pPos + offset;

                // Constraints:
                // 1. Above terrain
                if (terrain) {
                    float h = std::get<0>(terrain->GetTerrainPropertiesAtPoint(candidate.x, candidate.z));
                    if (candidate.y < h + 5.0f) continue;
                }

                // 2. Path Line of Sight (collision avoidance for camera movement)
                float hitT;
                if (CheckCollision(cameraSpline.waypoints.back(), candidate, terrain, decor, hitT)) {
                    continue;
                }

                // 3. Line of sight to Probe
                if (CheckCollision(candidate, pPos, terrain, decor, hitT)) {
                    if (hitT < 0.95f) continue;
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

            Vector3 targetFocus = pPos; // Default focus on probe

            // Check if probe is in a valid focus position (horizon angle constraint)
            glm::vec3 toProbe = glm::normalize(glm::vec3(pPos.x - cPos.x, pPos.y - cPos.y, pPos.z - cPos.z));
            float probeAngle = asin(toProbe.y);
            bool probeValid = (abs(probeAngle) <= 3.14159f / 4.0f);

            // Occasionally look at POIs, or if probe is invalid
            if (!probeValid || rand() % 3 == 0) {
                std::vector<Vector3> pois;

                // 1. Mountain peaks (proxies)
                if (terrain) {
                    auto chunks = terrain->GetVisibleChunks();
                    for (auto& chunk : chunks) {
                        pois.push_back(Vector3(chunk->proxy.highestPoint.x, chunk->proxy.highestPoint.y, chunk->proxy.highestPoint.z));
                    }
                }

                // 1.5 Sunset/Sun POI
                pois.push_back(cPos + Vector3(0, 10, -500)); // Simple "Sun" direction

                // 2. Decor structures
                if (decor && terrain) {
                    float worldScale = terrain->GetWorldScale();
                    float chunkSize = 32.0f * worldScale;
                    std::vector<std::pair<int, int>> chunks;
                    int cx = static_cast<int>(std::floor(cPos.x / chunkSize));
                    int cz = static_cast<int>(std::floor(cPos.z / chunkSize));
                    for(int dx=-2; dx<=2; ++dx) for(int dz=-2; dz<=2; ++dz) chunks.push_back({cx+dx, cz+dz});

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
                               if (pois.size() > 20) break;
                           }
                        }
                        if (pois.size() > 20) break;
                    }
                }

                // Pick the best POI
                float bestScore = -1e10f;
                for (const auto& poi : pois) {
                    // Check LoS from Camera
                    float hitT;
                    if (CheckCollision(cPos, poi, terrain, decor, hitT)) continue;

                    // Check horizon angle (max 45 degrees)
                    glm::vec3 toPoi = glm::normalize(glm::vec3(poi.x - cPos.x, poi.y - cPos.y, poi.z - cPos.z));
                    float angle = asin(toPoi.y);
                    if (abs(angle) > 3.14159f / 4.0f) continue;

                    // Score based on distance (not too far, not too close)
                    float d = (poi - cPos).Magnitude();
                    float score = -abs(d - 60.0f);

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
