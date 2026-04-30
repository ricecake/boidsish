#pragma once

#include <glm/glm.hpp>

namespace Boidsish {
	namespace Constants {
		namespace UboBinding {
			consteval int Lighting() {
				return 0;
			}

			consteval int VisualEffects() {
				return 1;
			}

			consteval int Shadows() {
				return 2;
			}

			consteval int FrustumData() {
				return 3;
			}

			consteval int Shockwaves() {
				return 4;
			}

			consteval int SdfVolumes() {
				return 5;
			}

			consteval int TemporalData() {
				return 6;
			}

			consteval int Biomes() {
				return 7;
			}

			consteval int TerrainData() {
				return 8;
			}

			consteval int WeatherUniforms() {
				return 32;
			}

			consteval int DecorProps() {
				return 30;
			}

			consteval int DecorPlacementGlobals() {
				return 31;
			}
		} // namespace UboBinding

		namespace SsboBinding {
			consteval int AutoExposure() {
				return 11;
			}

			consteval int BoneMatrix() {
				return 12;
			}

			consteval int OcclusionVisibility() {
				return 13;
			}

			consteval int ParticleGridHeads() {
				return 14;
			}

			consteval int ParticleGridNext() {
				return 15;
			}

			consteval int TrailPoints() {
				return 18;
			}

			consteval int TrailInstances() {
				return 19;
			}

			consteval int TrailSpineData() {
				return 20;
			}

			consteval int DecorChunkParams() {
				return 26;
			}

			consteval int VisibleParticleIndices() {
				return 27;
			}

			consteval int ParticleDrawCommand() {
				return 28;
			}

			consteval int ParticleBuffer() {
				return 16;
			}

			consteval int EmitterBuffer() {
				return 22;
			}

			consteval int IndirectionBuffer() {
				return 17;
			}

			consteval int TerrainChunkInfo() {
				return 23;
			}

			consteval int SliceData() {
				return 24;
			}

			consteval int TerrainProbes() {
				return 29;
			}

			consteval int LiveParticleIndices() {
				return 33;
			}

			consteval int BehaviorDrawCommand() {
				return 34;
			}

			consteval int WeatherGridA() {
				return 37;
			}

			consteval int WeatherGridB() {
				return 38;
			}
		} // namespace SsboBinding

		namespace General {
			namespace Math {
				consteval float Pi() {
					return 3.14159265358979323846f;
				}
			} // namespace Math

			namespace Colors {
				// Default ambient light color: glm::vec3(90.0f/255.0f, 81.0f/255.0f, 62.0f/255.0f)
				constexpr float DefaultAmbientR = 90.0f / 255.0f;
				constexpr float DefaultAmbientG = 81.0f / 255.0f;
				constexpr float DefaultAmbientB = 62.0f / 255.0f;

				consteval glm::vec3 DefaultAmbient() {
					return glm::vec3(DefaultAmbientR, DefaultAmbientG, DefaultAmbientB);
				}
			} // namespace Colors
		} // namespace General

		namespace Project {
			namespace Window {
				consteval int DefaultWidth() {
					return 1280;
				}

				consteval int DefaultHeight() {
					return 720;
				}
			} // namespace Window

			namespace Camera {
				consteval float DefaultFOV() {
					return 45.0f;
				}

				consteval float DefaultNearPlane() {
					return 0.1f;
				}

				/**
				 * @brief The clipping distance for the camera.
				 * Derived from MaxViewDistance and ChunkSize to ensure all loaded terrain is visible.
				 */
				constexpr float DefaultFarPlane();

				consteval float MinHeight() {
					return 0.1f;
				}

				consteval float MinSpeed() {
					return 0.5f;
				}

				consteval float DefaultSpeed() {
					return 10.0f;
				}

				consteval float SpeedStep() {
					return 2.5f;
				}

				consteval float RollSpeed() {
					return 45.0f;
				}

				// Path following
				consteval float DefaultPathSpeed() {
					return 20.0f;
				}

				consteval float PathBankFactor() {
					return 1.8f;
				}

				consteval float PathBankSpeed() {
					return 3.5f;
				}

				consteval float ChaseTrailBehind() {
					return 15.0f;
				}

				consteval float ChaseElevation() {
					return 5.0f;
				}

				consteval float ChaseLookAhead() {
					return 10.0f;
				}

				consteval float ChaseResponsiveness() {
					return 1.5f;
				}

				consteval float PathFollowSmoothing() {
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
			namespace Terrain {
				constexpr int SliceMapSize() {
					return 512;
				}
			} // namespace Terrain

			namespace Shadows {
				consteval int MaxLights() {
					return 10;
				}

				consteval int MaxCascades() {
					return 4;
				}

				consteval int MaxShadowMaps() {
					return 16;
				}

				consteval int MapSize() {
					return 2048;
				}

				consteval float DefaultSceneRadius() {
					return 500.0f;
				}

				consteval float DefaultFOV() {
					return 45.0f;
				}

				// Cascade split distances (logarithmic distribution)
				// Near splits are tighter for crisp close shadows
				// Far cascade acts as catchall for distant terrain
				consteval float CascadeSplit0() {
					return 20.0f;
				}

				consteval float CascadeSplit1() {
					return 50.0f;
				}

				consteval float CascadeSplit2() {
					return 150.0f;
				}

				consteval float CascadeSplit3() {
					return 700.0f;
				}

				// Grid snapping sizes per cascade (finer for near, coarser for far)
				consteval float GridSnapCascade0() {
					return 0.25f;
				}

				consteval float GridSnapCascade1() {
					return 1.0f;
				}

				consteval float GridSnapCascade2() {
					return 4.0f;
				}

				consteval float GridSnapCascade3() {
					return 8.0f;
				}
			} // namespace Shadows

			namespace Terrain {
				consteval int ChunkSize() {
					return 32;
				}

				consteval int ChunkSizePlus1() {
					return ChunkSize() + 1;
				}

				consteval int DefaultViewDistance() {
					return (32 * 10) / ChunkSize();
				}

				consteval int MaxViewDistance() {
					return (32 * 24) / ChunkSize();
				}

				consteval int UnloadDistanceBuffer() {
					return (32 * 12) / ChunkSize();
				}

				consteval int DefaultOctaves() {
					return 4;
				}

				consteval float ControlNoiseScale() {
					return 0.001f;
				}

				consteval float PathFrequency() {
					return 0.002f;
				}

				consteval float DefaultLacunarity() {
					return 0.99f;
				}

				consteval float DefaultPersistence() {
					return 0.5f;
				}

				consteval float PathCorridorWidth() {
					return 0.35f;
				}

				consteval float WarpStrength() {
					return 20.0f;
				}
			} // namespace Terrain

			namespace Particles {
				consteval int MaxParticles() {
					return 64000;
				}

				consteval int MaxEmitters() {
					return 100;
				}

				consteval int ComputeGroupSize() {
					return 256;
				}

				consteval int ParticleGridSize() {
					return 131072;
				}

				consteval float ParticleGridCellSize() {
					return 2.0f;
				}

				consteval float DefaultAmbientDensity() {
					return 0.15f;
				}
			} // namespace Particles

			namespace Explosions {
				consteval int MaxFragments() {
					return 50000;
				}

				consteval int ComputeGroupSize() {
					return 64;
				}

				consteval float DefaultVelocity() {
					return 10.0f;
				}

				consteval float DefaultRandomVelocity() {
					return 5.0f;
				}
			} // namespace Explosions

			namespace Shockwaves {
				consteval int MaxShockwaves() {
					return 16;
				}

				consteval float DefaultIntensity() {
					return 0.5f;
				}

				consteval float DefaultRingWidth() {
					return 3.0f;
				}

				consteval float DefaultDuration() {
					return 1.2f;
				} // Based on CreateExplosion logic

				consteval glm::vec3 DefaultColor() {
					return glm::vec3(1.0f, 0.6f, 0.2f);
				}
			} // namespace Shockwaves

			namespace SdfVolumes {
				consteval int MaxSources() {
					return 128;
				}

				consteval float DefaultRadius() {
					return 5.0f;
				}

				consteval float DefaultSmoothness() {
					return 2.0f;
				}
			} // namespace SdfVolumes

			namespace Trails {
				consteval int DefaultMaxLength() {
					return 250;
				}

				consteval int DefaultTrailLength() {
					return 10;
				}

				consteval int Segments() {
					return 8;
				}

				consteval int CurveSegments() {
					return 4;
				}

				consteval float BaseThickness() {
					return 0.06f;
				}

				consteval float DefaultRoughness() {
					return 0.3f;
				}

				consteval float DefaultMetallic() {
					return 0.0f;
				}

				consteval int FloatsPerVertex() {
					return 9;
				}

				consteval int InitialVertexCapacity() {
					return 500000;
				}

				consteval float GrowthFactor() {
					return 1.5f;
				}
			} // namespace Trails

			namespace Shapes {
				namespace Arrow {
					consteval float DefaultConeHeight() {
						return 0.2f;
					}

					consteval float DefaultConeRadius() {
						return 0.1f;
					}

					consteval float DefaultRodRadius() {
						return 0.05f;
					}
				} // namespace Arrow
			} // namespace Shapes

			namespace Rendering {
				consteval int BlurPasses() {
					return 4;
				}
			} // namespace Rendering

			namespace Akira {
				consteval float DefaultGrowthDuration() {
					return 0.5f;
				}

				consteval float DefaultFadeDuration() {
					return 3.0f;
				}

				consteval float DefaultRadius() {
					return 20.0f;
				}
			} // namespace Akira

			namespace Megabuffer {
				consteval size_t MaxVertices() {
					return 1000000;
				}

				consteval size_t MaxIndices() {
					return 2000000;
				}

				consteval size_t StaticVertexLimit() {
					return 500000;
				}

				consteval size_t StaticIndexLimit() {
					return 1000000;
				}
			} // namespace Megabuffer

			namespace Checkpoint {
				consteval float DefaultRadius() {
					return 10.0f;
				}

				consteval float DefaultHaloWidth() {
					return 2.0f;
				}

				consteval float DefaultAuraWidth() {
					return 5.0f;
				}

				consteval float DefaultLifespan() {
					return 60.0f;
				}

				namespace Colors {
					consteval float GoldR() { return 1.0f; }
					consteval float GoldG() { return 0.84f; }
					consteval float GoldB() { return 0.0f; }
					consteval float SilverR() { return 0.75f; }
					consteval float SilverG() { return 0.75f; }
					consteval float SilverB() { return 0.75f; }
					consteval float BlackR() { return 0.01f; }
					consteval float BlackG() { return 0.01f; }
					consteval float BlackB() { return 0.01f; }
					consteval float BlueR() { return 0.0f; }
					consteval float BlueG() { return 0.5f; }
					consteval float BlueB() { return 1.0f; }
					consteval float NeonGreenR() { return 0.2f; }
					consteval float NeonGreenG() { return 1.0f; }
					consteval float NeonGreenB() { return 0.2f; }

					consteval glm::vec3 Gold() {
						return glm::vec3(GoldR(), GoldG(), GoldB());
					}

					consteval glm::vec3 Silver() {
						return glm::vec3(SilverR(), SilverG(), SilverB());
					}

					consteval glm::vec3 Black() {
						return glm::vec3(BlackR(), BlackG(), BlackB());
					}

					consteval glm::vec3 Blue() {
						return glm::vec3(BlueR(), BlueG(), BlueB());
					}

					consteval glm::vec3 NeonGreen() {
						return glm::vec3(NeonGreenR(), NeonGreenG(), NeonGreenB());
					}
				} // namespace Colors
			} // namespace Checkpoint
		} // namespace Class
	} // namespace Constants

	namespace Constants::Project::Camera {
		constexpr float DefaultFarPlane() {
			return static_cast<float>(Class::Terrain::MaxViewDistance() * Class::Terrain::ChunkSize());
		}
	} // namespace Constants::Project::Camera
} // namespace Boidsish
