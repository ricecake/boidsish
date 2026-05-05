#pragma once

#include <utility>

#include <glm/glm.hpp>

namespace Boidsish {
	namespace Constants {
		namespace Bindings {
			enum class Texture {
				ShadowMaps = 10,
				TerrainChunkGrid = 11,
				TerrainMaxHeight = 12,
				TerrainHeightmap = 13,
				TerrainBiomeMap = 14,
				Refraction = 15,
				NoiseSimplex = 16,
				NoiseCurl = 17,
				NoiseBlue = 18,
				NoiseExtra = 19,
				NoisePhasor = 20,
				AtmosphereTransmittance = 21,
				AtmosphereMultiScattering = 22,
				AtmosphereSkyView = 23,
				AtmosphereAerialPerspective = 24,
				AtmosphereCloudShadow = 25,
				WindData = 26,
				HiZ = 27,
				TerrainBakedParams = 28,
				TerrainRawHeightmap = 29,
				TerrainBiomeImage = 30,
				TerrainHeightmapImage = 31,
				TerrainBakedParamsImage = 32,
				TerrainHorizonMap = 33,
				LbmWindData = 34,
				TerrainShadowMap = 35,
				TerrainShadowMapImage = 36,
				WeatherScalars = 42,
			};

			enum class Ubo {
				Lighting = 0,
				VisualEffects = 1,
				Shadows = 2,
				FrustumData = 3,
				Shockwaves = 4,
				TemporalData = 6,
				Biomes = 7,
				TerrainData = 8,
				WeatherUniforms = 32,
				GrassProps = 9,
				DecorProps = 30,
				DecorPlacementGlobals = 31,
				WindData = 45,
			};

			enum class Ssbo {
				DecorInstances = 10,
				AutoExposure = 11,
				BoneMatrix = 12,
				OcclusionVisibility = 13,
				ParticleGridHeads = 14,
				ParticleGridNext = 15,
				ParticleBuffer = 16,
				IndirectionBuffer = 17,
				TrailPoints = 18,
				TrailInstances = 19,
				TrailSpineData = 20,
				CommonUniforms = 21,
				EmitterBuffer = 22,
				TerrainChunkInfo = 23,
				SliceData = 24,
				DecorAllInstances = 25,
				DecorChunkParams = 26,
				VisibleParticleIndices = 27,
				ParticleDrawCommand = 28,
				TerrainProbes = 29,
				SdfVolumes = 44,
				LiveParticleIndices = 33,
				BehaviorDrawCommand = 34,
				GrassInstances = 35,
				GrassIndirect = 36,
				WeatherGridA = 37,
				WeatherGridB = 38,
				DecorIndirect = 39,
				DecorBlockValidity = 40,
				MeshExplosionFragments = 41,
				TrailGeneratedVBO = 42,
				AtmosphereSH = 43,
			};
		} // namespace Bindings

		namespace UboBinding {
			constexpr int Lighting() {
				return static_cast<int>(Constants::Bindings::Ubo::Lighting);
			}

			constexpr int VisualEffects() {
				return static_cast<int>(Constants::Bindings::Ubo::VisualEffects);
			}

			constexpr int Shadows() {
				return static_cast<int>(Constants::Bindings::Ubo::Shadows);
			}

			constexpr int FrustumData() {
				return static_cast<int>(Constants::Bindings::Ubo::FrustumData);
			}

			constexpr int Shockwaves() {
				return static_cast<int>(Constants::Bindings::Ubo::Shockwaves);
			}

			constexpr int TemporalData() {
				return static_cast<int>(Constants::Bindings::Ubo::TemporalData);
			}

			constexpr int Biomes() {
				return static_cast<int>(Constants::Bindings::Ubo::Biomes);
			}

			constexpr int TerrainData() {
				return static_cast<int>(Constants::Bindings::Ubo::TerrainData);
			}

			constexpr int WeatherUniforms() {
				return static_cast<int>(Constants::Bindings::Ubo::WeatherUniforms);
			}

			constexpr int GrassProps() {
				return static_cast<int>(Constants::Bindings::Ubo::GrassProps);
			}

			constexpr int DecorProps() {
				return static_cast<int>(Constants::Bindings::Ubo::DecorProps);
			}

			constexpr int DecorPlacementGlobals() {
				return static_cast<int>(Constants::Bindings::Ubo::DecorPlacementGlobals);
			}

			constexpr int WindData() {
				return static_cast<int>(Constants::Bindings::Ubo::WindData);
			}
		} // namespace UboBinding

		namespace TextureUnit {
			constexpr int ShadowMaps() {
				return static_cast<int>(Constants::Bindings::Texture::ShadowMaps);
			}

			constexpr int TerrainChunkGrid() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainChunkGrid);
			}

			constexpr int TerrainMaxHeight() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainMaxHeight);
			}

			constexpr int TerrainHeightmap() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainHeightmap);
			}

			constexpr int TerrainBiomeMap() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainBiomeMap);
			}

			constexpr int Refraction() {
				return static_cast<int>(Constants::Bindings::Texture::Refraction);
			}

			constexpr int NoiseSimplex() {
				return static_cast<int>(Constants::Bindings::Texture::NoiseSimplex);
			}

			constexpr int NoiseCurl() {
				return static_cast<int>(Constants::Bindings::Texture::NoiseCurl);
			}

			constexpr int NoiseBlue() {
				return static_cast<int>(Constants::Bindings::Texture::NoiseBlue);
			}

			constexpr int NoiseExtra() {
				return static_cast<int>(Constants::Bindings::Texture::NoiseExtra);
			}

			constexpr int NoisePhasor() {
				return static_cast<int>(Constants::Bindings::Texture::NoisePhasor);
			}

			constexpr int AtmosphereTransmittance() {
				return static_cast<int>(Constants::Bindings::Texture::AtmosphereTransmittance);
			}

			constexpr int AtmosphereMultiScattering() {
				return static_cast<int>(Constants::Bindings::Texture::AtmosphereMultiScattering);
			}

			constexpr int AtmosphereSkyView() {
				return static_cast<int>(Constants::Bindings::Texture::AtmosphereSkyView);
			}

			constexpr int AtmosphereAerialPerspective() {
				return static_cast<int>(Constants::Bindings::Texture::AtmosphereAerialPerspective);
			}

			constexpr int AtmosphereCloudShadow() {
				return static_cast<int>(Constants::Bindings::Texture::AtmosphereCloudShadow);
			}

			constexpr int WindData() {
				return static_cast<int>(Constants::Bindings::Texture::WindData);
			}

			constexpr int HiZ() {
				return static_cast<int>(Constants::Bindings::Texture::HiZ);
			}

			constexpr int TerrainBakedParams() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainBakedParams);
			}

			constexpr int TerrainRawHeightmap() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainRawHeightmap);
			}

			constexpr int TerrainBiomeImage() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainBiomeImage);
			}

			constexpr int TerrainHeightmapImage() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainHeightmapImage);
			}

			constexpr int TerrainBakedParamsImage() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainBakedParamsImage);
			}

			constexpr int TerrainHorizonMap() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainHorizonMap);
			}

			constexpr int LbmWindData() {
				return static_cast<int>(Constants::Bindings::Texture::LbmWindData);
			}

			constexpr int TerrainShadowMap() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainShadowMap);
			}

			constexpr int TerrainShadowMapImage() {
				return static_cast<int>(Constants::Bindings::Texture::TerrainShadowMapImage);
			}

			constexpr int WeatherScalars() {
				return static_cast<int>(Constants::Bindings::Texture::WeatherScalars);
			}
		} // namespace TextureUnit

		namespace SsboBinding {
			constexpr int DecorInstances() {
				return static_cast<int>(Constants::Bindings::Ssbo::DecorInstances);
			}

			constexpr int AutoExposure() {
				return static_cast<int>(Constants::Bindings::Ssbo::AutoExposure);
			}

			constexpr int BoneMatrix() {
				return static_cast<int>(Constants::Bindings::Ssbo::BoneMatrix);
			}

			constexpr int OcclusionVisibility() {
				return static_cast<int>(Constants::Bindings::Ssbo::OcclusionVisibility);
			}

			constexpr int ParticleGridHeads() {
				return static_cast<int>(Constants::Bindings::Ssbo::ParticleGridHeads);
			}

			constexpr int ParticleGridNext() {
				return static_cast<int>(Constants::Bindings::Ssbo::ParticleGridNext);
			}

			constexpr int ParticleBuffer() {
				return static_cast<int>(Constants::Bindings::Ssbo::ParticleBuffer);
			}

			constexpr int IndirectionBuffer() {
				return static_cast<int>(Constants::Bindings::Ssbo::IndirectionBuffer);
			}

			constexpr int TrailPoints() {
				return static_cast<int>(Constants::Bindings::Ssbo::TrailPoints);
			}

			constexpr int TrailInstances() {
				return static_cast<int>(Constants::Bindings::Ssbo::TrailInstances);
			}

			constexpr int TrailSpineData() {
				return static_cast<int>(Constants::Bindings::Ssbo::TrailSpineData);
			}

			constexpr int CommonUniforms() {
				return static_cast<int>(Constants::Bindings::Ssbo::CommonUniforms);
			}

			constexpr int EmitterBuffer() {
				return static_cast<int>(Constants::Bindings::Ssbo::EmitterBuffer);
			}

			constexpr int TerrainChunkInfo() {
				return static_cast<int>(Constants::Bindings::Ssbo::TerrainChunkInfo);
			}

			constexpr int SliceData() {
				return static_cast<int>(Constants::Bindings::Ssbo::SliceData);
			}

			constexpr int DecorAllInstances() {
				return static_cast<int>(Constants::Bindings::Ssbo::DecorAllInstances);
			}

			constexpr int DecorChunkParams() {
				return static_cast<int>(Constants::Bindings::Ssbo::DecorChunkParams);
			}

			constexpr int VisibleParticleIndices() {
				return static_cast<int>(Constants::Bindings::Ssbo::VisibleParticleIndices);
			}

			constexpr int ParticleDrawCommand() {
				return static_cast<int>(Constants::Bindings::Ssbo::ParticleDrawCommand);
			}

			constexpr int TerrainProbes() {
				return static_cast<int>(Constants::Bindings::Ssbo::TerrainProbes);
			}

			constexpr int SdfVolumes() {
				return static_cast<int>(Constants::Bindings::Ssbo::SdfVolumes);
			}

			constexpr int LiveParticleIndices() {
				return static_cast<int>(Constants::Bindings::Ssbo::LiveParticleIndices);
			}

			constexpr int BehaviorDrawCommand() {
				return static_cast<int>(Constants::Bindings::Ssbo::BehaviorDrawCommand);
			}

			constexpr int GrassInstances() {
				return static_cast<int>(Constants::Bindings::Ssbo::GrassInstances);
			}

			constexpr int GrassIndirect() {
				return static_cast<int>(Constants::Bindings::Ssbo::GrassIndirect);
			}

			constexpr int WeatherGridA() {
				return static_cast<int>(Constants::Bindings::Ssbo::WeatherGridA);
			}

			constexpr int WeatherGridB() {
				return static_cast<int>(Constants::Bindings::Ssbo::WeatherGridB);
			}

			constexpr int DecorIndirect() {
				return static_cast<int>(Constants::Bindings::Ssbo::DecorIndirect);
			}

			constexpr int DecorBlockValidity() {
				return static_cast<int>(Constants::Bindings::Ssbo::DecorBlockValidity);
			}

			constexpr int MeshExplosionFragments() {
				return static_cast<int>(Constants::Bindings::Ssbo::MeshExplosionFragments);
			}

			constexpr int TrailGeneratedVBO() {
				return static_cast<int>(Constants::Bindings::Ssbo::TrailGeneratedVBO);
			}

			constexpr int AtmosphereSH() {
				return static_cast<int>(Constants::Bindings::Ssbo::AtmosphereSH);
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

				/**
				 * @brief The clipping distance for the camera.
				 * Derived from MaxViewDistance and ChunkSize to ensure all loaded terrain is visible.
				 */
				constexpr float DefaultFarPlane();

				constexpr float MinHeight() {
					return 0.1f;
				}

				constexpr float MinSpeed() {
					return 0.5f;
				}

				constexpr float DefaultSpeed() {
					return 15.0f;
				}

				constexpr float FirstPersonEyeHeight() {
					return 4.8f;
				}

				constexpr float FirstPersonCrouchHeight() {
					return 1.5f;
				}

				constexpr float FirstPersonSprintMultiplier() {
					return 2.0f;
				}

				constexpr float FirstPersonJumpForce() {
					return 12.5f;
				}

				constexpr float FirstPersonGravity() {
					return 18.0f;
				}

				constexpr float FirstPersonGroundSmoothing() {
					return 5.0f;
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
			namespace Terrain {
				constexpr int SliceMapSize() {
					return 512;
				}
			} // namespace Terrain

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

				constexpr int ChunkSizePlus1() {
					return ChunkSize() + 1;
				}

				constexpr int DefaultViewDistance() {
					return (32 * 10) / ChunkSize();
				}

				constexpr int MaxViewDistance() {
					return (32 * 24) / ChunkSize();
				}

				constexpr int UnloadDistanceBuffer() {
					return (32 * 12) / ChunkSize();
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
					return 0.15f;
				}

				constexpr float WarpStrength() {
					return 20.0f;
				}
			} // namespace Terrain

			namespace Particles {
				constexpr int MaxParticles() {
					return 128000;
				}

				constexpr int MaxEmitters() {
					return 100;
				}

				constexpr int ComputeGroupSize() {
					return 256;
				}

				constexpr int ParticleGridSize() {
					return 131072;
				}

				constexpr float ParticleGridCellSize() {
					return 2.0f;
				}

				constexpr float DefaultAmbientDensity() {
					return 0.15f;
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
					return 2000000;
				}

				constexpr size_t MaxIndices() {
					return 4000000;
				}

				constexpr size_t StaticVertexLimit() {
					return 1000000;
				}

				constexpr size_t StaticIndexLimit() {
					return 2000000;
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
					constexpr float GoldR() {
						return 1.0f;
					}

					constexpr float GoldG() {
						return 0.84f;
					}

					constexpr float GoldB() {
						return 0.0f;
					}

					constexpr float SilverR() {
						return 0.75f;
					}

					constexpr float SilverG() {
						return 0.75f;
					}

					constexpr float SilverB() {
						return 0.75f;
					}

					constexpr float BlackR() {
						return 0.01f;
					}

					constexpr float BlackG() {
						return 0.01f;
					}

					constexpr float BlackB() {
						return 0.01f;
					}

					constexpr float BlueR() {
						return 0.0f;
					}

					constexpr float BlueG() {
						return 0.5f;
					}

					constexpr float BlueB() {
						return 1.0f;
					}

					constexpr float NeonGreenR() {
						return 0.2f;
					}

					constexpr float NeonGreenG() {
						return 1.0f;
					}

					constexpr float NeonGreenB() {
						return 0.2f;
					}

					constexpr glm::vec3 Gold() {
						return glm::vec3(GoldR(), GoldG(), GoldB());
					}

					constexpr glm::vec3 Silver() {
						return glm::vec3(SilverR(), SilverG(), SilverB());
					}

					constexpr glm::vec3 Black() {
						return glm::vec3(BlackR(), BlackG(), BlackB());
					}

					constexpr glm::vec3 Blue() {
						return glm::vec3(BlueR(), BlueG(), BlueB());
					}

					constexpr glm::vec3 NeonGreen() {
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
