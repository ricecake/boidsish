#pragma once

#include <glm/glm.hpp>

namespace Boidsish {
	namespace Constants {
		namespace General {
			namespace Math {
				consteval float Pi() { return 3.14159265358979323846f; }
			}

			namespace Colors {
				// Default ambient light color: glm::vec3(90.0f/255.0f, 81.0f/255.0f, 62.0f/255.0f)
				constexpr float DefaultAmbientR = 90.0f / 255.0f;
				constexpr float DefaultAmbientG = 81.0f / 255.0f;
				constexpr float DefaultAmbientB = 62.0f / 255.0f;

				consteval glm::vec3 DefaultAmbient() {
					return glm::vec3(DefaultAmbientR, DefaultAmbientG, DefaultAmbientB);
				}
			}
		} // namespace General

		namespace Project {
			namespace Window {
				consteval int DefaultWidth() { return 1280; }
				consteval int DefaultHeight() { return 720; }
			}

			namespace Camera {
				consteval float DefaultFOV() { return 45.0f; }
				consteval float DefaultNearPlane() { return 0.1f; }
				consteval float DefaultFarPlane() { return 1000.0f; }
				consteval float MinHeight() { return 0.1f; }
				consteval float MinSpeed() { return 0.5f; }
				consteval float DefaultSpeed() { return 10.0f; }
				consteval float SpeedStep() { return 2.5f; }
				consteval float RollSpeed() { return 45.0f; }

				// Path following
				consteval float DefaultPathSpeed() { return 20.0f; }
				consteval float PathBankFactor() { return 1.8f; }
				consteval float PathBankSpeed() { return 3.5f; }
			}
		} // namespace Project

		namespace Library {
			namespace Input {
				consteval int MaxKeys() { return 1024; }
				consteval int MaxMouseButtons() { return 8; }
			}
		} // namespace Library

		namespace Class {
			namespace Shadows {
				consteval int   MaxLights() { return 4; }
				consteval int   MapSize() { return 2048; }
				consteval float DefaultSceneRadius() { return 500.0f; }
				consteval float UpdateDistanceThreshold() { return 200.0f; }
				consteval float GridSnappingSize() { return 10.0f; }
			}

			namespace Terrain {
				consteval int   ChunkSize() { return 32; }
				consteval int   DefaultViewDistance() { return 10; }
				consteval int   MaxViewDistance() { return 24; }
				consteval int   UnloadDistanceBuffer() { return 12; }
				consteval int   DefaultOctaves() { return 4; }
				consteval float ControlNoiseScale() { return 0.001f; }
				consteval float PathFrequency() { return 0.002f; }
				consteval float DefaultLacunarity() { return 0.99f; }
				consteval float DefaultPersistence() { return 0.5f; }
				consteval float PathCorridorWidth() { return 0.35f; }
				consteval float WarpStrength() { return 20.0f; }
			}

			namespace Particles {
				consteval int MaxParticles() { return 64000; }
				consteval int MaxEmitters() { return 100; }
				consteval int ComputeGroupSize() { return 256; }
			}

			namespace Explosions {
				consteval int MaxFragments() { return 50000; }
				consteval int ComputeGroupSize() { return 64; }
				consteval float DefaultVelocity() { return 10.0f; }
				consteval float DefaultRandomVelocity() { return 5.0f; }
			}

			namespace Shockwaves {
				consteval int   MaxShockwaves() { return 16; }
				consteval float DefaultIntensity() { return 0.5f; }
				consteval float DefaultRingWidth() { return 3.0f; }
				consteval float DefaultDuration() { return 1.2f; } // Based on CreateExplosion logic

				consteval glm::vec3 DefaultColor() { return glm::vec3(1.0f, 0.6f, 0.2f); }
			}

			namespace Trails {
				consteval int   DefaultMaxLength() { return 250; }
				consteval int   DefaultTrailLength() { return 10; }
				consteval int   Segments() { return 8; }
				consteval int   CurveSegments() { return 4; }
				consteval float BaseThickness() { return 0.06f; }
				consteval float DefaultRoughness() { return 0.3f; }
				consteval float DefaultMetallic() { return 0.0f; }
				consteval int   FloatsPerVertex() { return 9; }
				consteval int   InitialVertexCapacity() { return 500000; }
				consteval float GrowthFactor() { return 1.5f; }
			}

			namespace Shapes {
				namespace Arrow {
					consteval float DefaultConeHeight() { return 0.2f; }
					consteval float DefaultConeRadius() { return 0.1f; }
					consteval float DefaultRodRadius() { return 0.05f; }
				}
			}

			namespace Rendering {
				consteval int BlurPasses() { return 4; }
			}
		} // namespace Class
	}     // namespace Constants
} // namespace Boidsish
