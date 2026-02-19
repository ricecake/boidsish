#pragma once

#include <glm/glm.hpp>

namespace Boidsish {
	namespace Constants {
		namespace UboBinding {
			constexpr int Lighting() {
				return 0;
			}

			constexpr int VisualEffects() {
				return 1;
			}

			constexpr int Shadows() {
				return 2;
			}

			constexpr int FrustumData() {
				return 3;
			}

			constexpr int Shockwaves() {
				return 4;
			}

			constexpr int SdfVolumes() {
				return 5;
			}
		} // namespace UboBinding

		namespace SsboBinding {
			constexpr int AutoExposure() {
				return 11;
			}
		} // namespace SsboBinding

		namespace General {
			namespace Math {
				constexpr float Pi() {
					return 3.14159265358979323846f;
				}
			} // namespace Math

			namespace Colors {
				// Default ambient light color: glm::vec3(90.0f/255.0f, 81.0f/255.0f, 62.0f/255.0f)
				constexpr float DefaultAmbientR = 90.0f / 255.0f;
				constexpr float DefaultAmbientG = 81.0f / 255.0f;
				constexpr float DefaultAmbientB = 62.0f / 255.0f;

				constexpr glm::vec3 DefaultAmbient() {
					return glm::vec3(DefaultAmbientR, DefaultAmbientG, DefaultAmbientB);
				}
			} // namespace Colors
		} // namespace General

		namespace Project {
			namespace Window {
				constexpr int DefaultWidth() {
					return 1280;
				}

				constexpr int DefaultHeight() {
					return 720;
				}
			} // namespace Window

			namespace Camera {
				constexpr float DefaultFOV() {
					return 45.0f;
				}

				constexpr float DefaultNearPlane() {
					return 0.1f;
				}

				constexpr float DefaultFarPlane() {
					return 1000.0f;
				}

				constexpr float MinHeight() {
					return 0.1f;
				}

				constexpr float MinSpeed() {
					return 0.5f;
				}

				constexpr float DefaultSpeed() {
					return 10.0f;
				}

				constexpr float SpeedStep() {
					return 2.5f;
				}

				constexpr float RollSpeed() {
					return 45.0f;
				}

				// Path following
				constexpr float DefaultPathSpeed() {
					return 20.0f;
				}

				constexpr float PathBankFactor() {
					return 1.8f;
				}

				constexpr float PathBankSpeed() {
					return 3.5f;
				}

				constexpr float ChaseTrailBehind() {
					return 15.0f;
				}

				constexpr float ChaseElevation() {
					return 5.0f;
				}

				constexpr float ChaseLookAhead() {
					return 10.0f;
				}

				constexpr float ChaseResponsiveness() {
					return 1.5f;
				}

				constexpr float PathFollowSmoothing() {
					return 5.0f;
				}

				// glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
				// glm::vec3 desired_cam_pos = target_pos - forward * 15.0f + up * 5.0f;
				// glm::vec3 look_at_pos = target_pos + forward * 10.0f;

				// // 3. Smoothly interpolate camera position
				// // Frame-rate independent interpolation using exponential decay
				// float     lerp_factor = 1.0f - exp(-delta_time * 2.5f);

				// glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
				// glm::vec3 desired_cam_pos = target_pos - forward * camera.follow_distance + up *
				// camera.follow_elevation; glm::vec3 look_at_pos = target_pos + forward * camera.follow_look_ahead;

				// // 3. Smoothly interpolate camera position
				// // Frame-rate independent interpolation using exponential decay
				// float     lerp_factor = 1.0f - exp(-delta_time * camera.follow_responsiveness);
				// glm::vec3 new_cam_pos = glm::mix(camera.pos(), desired_cam_pos, lerp_factor);

			} // namespace Camera
		} // namespace Project

		namespace Library {
			namespace Input {
				constexpr int MaxKeys() {
					return 1024;
				}

				constexpr int MaxMouseButtons() {
					return 8;
				}
			} // namespace Input
		} // namespace Library

		namespace Class {
			namespace Shadows {
				constexpr int MaxLights() {
					return 10;
				}

				constexpr int MaxCascades() {
					return 4;
				}

				constexpr int MaxShadowMaps() {
					return 16;
				}

				constexpr int MapSize() {
					return 2048;
				}

				constexpr float DefaultSceneRadius() {
					return 500.0f;
				}

				constexpr float DefaultFOV() {
					return 45.0f;
				}

				// Cascade split distances (logarithmic distribution)
				// Near splits are tighter for crisp close shadows
				// Far cascade acts as catchall for distant terrain
				constexpr float CascadeSplit0() {
					return 20.0f;
				}

				constexpr float CascadeSplit1() {
					return 50.0f;
				}

				constexpr float CascadeSplit2() {
					return 150.0f;
				}

				constexpr float CascadeSplit3() {
					return 700.0f;
				}

				// Grid snapping sizes per cascade (finer for near, coarser for far)
				constexpr float GridSnapCascade0() {
					return 0.25f;
				}

				constexpr float GridSnapCascade1() {
					return 1.0f;
				}

				constexpr float GridSnapCascade2() {
					return 4.0f;
				}

				constexpr float GridSnapCascade3() {
					return 8.0f;
				}
			} // namespace Shadows

			namespace Terrain {
				constexpr int ChunkSize() {
					return 32;
				}

				constexpr int DefaultViewDistance() {
					return 10;
				}

				constexpr int MaxViewDistance() {
					return 24;
				}

				constexpr int UnloadDistanceBuffer() {
					return 12;
				}

				constexpr int DefaultOctaves() {
					return 4;
				}

				constexpr float ControlNoiseScale() {
					return 0.001f;
				}

				constexpr float PathFrequency() {
					return 0.002f;
				}

				constexpr float DefaultLacunarity() {
					return 0.99f;
				}

				constexpr float DefaultPersistence() {
					return 0.5f;
				}

				constexpr float PathCorridorWidth() {
					return 0.35f;
				}

				constexpr float WarpStrength() {
					return 20.0f;
				}
			} // namespace Terrain

			namespace Particles {
				constexpr int MaxParticles() {
					return 64000;
				}

				constexpr int MaxEmitters() {
					return 100;
				}

				constexpr int ComputeGroupSize() {
					return 256;
				}
			} // namespace Particles

			namespace Explosions {
				constexpr int MaxFragments() {
					return 50000;
				}

				constexpr int ComputeGroupSize() {
					return 64;
				}

				constexpr float DefaultVelocity() {
					return 10.0f;
				}

				constexpr float DefaultRandomVelocity() {
					return 5.0f;
				}
			} // namespace Explosions

			namespace Shockwaves {
				constexpr int MaxShockwaves() {
					return 16;
				}

				constexpr float DefaultIntensity() {
					return 0.5f;
				}

				constexpr float DefaultRingWidth() {
					return 3.0f;
				}

				constexpr float DefaultDuration() {
					return 1.2f;
				} // Based on CreateExplosion logic

				constexpr glm::vec3 DefaultColor() {
					return glm::vec3(1.0f, 0.6f, 0.2f);
				}
			} // namespace Shockwaves

			namespace SdfVolumes {
				constexpr int MaxSources() {
					return 128;
				}

				constexpr float DefaultRadius() {
					return 5.0f;
				}

				constexpr float DefaultSmoothness() {
					return 2.0f;
				}
			} // namespace SdfVolumes

			namespace Trails {
				constexpr int DefaultMaxLength() {
					return 250;
				}

				constexpr int DefaultTrailLength() {
					return 10;
				}

				constexpr int Segments() {
					return 8;
				}

				constexpr int CurveSegments() {
					return 4;
				}

				constexpr float BaseThickness() {
					return 0.06f;
				}

				constexpr float DefaultRoughness() {
					return 0.3f;
				}

				constexpr float DefaultMetallic() {
					return 0.0f;
				}

				constexpr int FloatsPerVertex() {
					return 9;
				}

				constexpr int InitialVertexCapacity() {
					return 500000;
				}

				constexpr float GrowthFactor() {
					return 1.5f;
				}
			} // namespace Trails

			namespace Shapes {
				namespace Arrow {
					constexpr float DefaultConeHeight() {
						return 0.2f;
					}

					constexpr float DefaultConeRadius() {
						return 0.1f;
					}

					constexpr float DefaultRodRadius() {
						return 0.05f;
					}
				} // namespace Arrow
			} // namespace Shapes

			namespace Rendering {
				constexpr int BlurPasses() {
					return 4;
				}
			} // namespace Rendering

			namespace Akira {
				constexpr float DefaultGrowthDuration() {
					return 0.5f;
				}

				constexpr float DefaultFadeDuration() {
					return 3.0f;
				}

				constexpr float DefaultRadius() {
					return 20.0f;
				}
			} // namespace Akira

			namespace Megabuffer {
				constexpr size_t MaxVertices() {
					return 1000000;
				}

				constexpr size_t MaxIndices() {
					return 2000000;
				}

				constexpr size_t StaticVertexLimit() {
					return 500000;
				}

				constexpr size_t StaticIndexLimit() {
					return 1000000;
				}
			} // namespace Megabuffer

			namespace Checkpoint {
				constexpr float DefaultRadius() {
					return 10.0f;
				}

				constexpr float DefaultHaloWidth() {
					return 2.0f;
				}

				constexpr float DefaultAuraWidth() {
					return 5.0f;
				}

				constexpr float DefaultLifespan() {
					return 60.0f;
				}

				namespace Colors {
					constexpr float GoldR = 1.0f, GoldG = 0.84f, GoldB = 0.0f;
					constexpr float SilverR = 0.75f, SilverG = 0.75f, SilverB = 0.75f;
					constexpr float BlackR = 0.01f, BlackG = 0.01f, BlackB = 0.01f;
					constexpr float BlueR = 0.0f, BlueG = 0.5f, BlueB = 1.0f;
					constexpr float NeonGreenR = 0.2f, NeonGreenG = 1.0f, NeonGreenB = 0.2f;

					constexpr glm::vec3 Gold() {
						return glm::vec3(GoldR, GoldG, GoldB);
					}

					constexpr glm::vec3 Silver() {
						return glm::vec3(SilverR, SilverG, SilverB);
					}

					constexpr glm::vec3 Black() {
						return glm::vec3(BlackR, BlackG, BlackB);
					}

					constexpr glm::vec3 Blue() {
						return glm::vec3(BlueR, BlueG, BlueB);
					}

					constexpr glm::vec3 NeonGreen() {
						return glm::vec3(NeonGreenR, NeonGreenG, NeonGreenB);
					}
				} // namespace Colors
			} // namespace Checkpoint
		} // namespace Class
	} // namespace Constants
} // namespace Boidsish
