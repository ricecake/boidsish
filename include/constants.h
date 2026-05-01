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
				TerrainShadowMap = 35,
				TerrainShadowMapImage = 36,
				VolumetricLightTexture = 38,
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
				VolumetricLighting = 48,
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
				VolumetricLightAccumulation = 46,
			};
		} // namespace Bindings

		namespace UboBinding {
			consteval int Lighting() {
				return std::to_underlying(Constants::Bindings::Ubo::Lighting);
			}

			consteval int VisualEffects() {
				return std::to_underlying(Constants::Bindings::Ubo::VisualEffects);
			}

			consteval int Shadows() {
				return std::to_underlying(Constants::Bindings::Ubo::Shadows);
			}

			consteval int FrustumData() {
				return std::to_underlying(Constants::Bindings::Ubo::FrustumData);
			}

			consteval int Shockwaves() {
				return std::to_underlying(Constants::Bindings::Ubo::Shockwaves);
			}

			consteval int TemporalData() {
				return std::to_underlying(Constants::Bindings::Ubo::TemporalData);
			}

			consteval int Biomes() {
				return std::to_underlying(Constants::Bindings::Ubo::Biomes);
			}

			consteval int TerrainData() {
				return std::to_underlying(Constants::Bindings::Ubo::TerrainData);
			}

			consteval int WeatherUniforms() {
				return std::to_underlying(Constants::Bindings::Ubo::WeatherUniforms);
			}

			consteval int GrassProps() {
				return std::to_underlying(Constants::Bindings::Ubo::GrassProps);
			}

			consteval int DecorProps() {
				return std::to_underlying(Constants::Bindings::Ubo::DecorProps);
			}

			consteval int DecorPlacementGlobals() {
				return std::to_underlying(Constants::Bindings::Ubo::DecorPlacementGlobals);
			}

			consteval int WindData() {
				return std::to_underlying(Constants::Bindings::Ubo::WindData);
			}

			consteval int VolumetricLighting() {
				return std::to_underlying(Constants::Bindings::Ubo::VolumetricLighting);
			}
		} // namespace UboBinding

		namespace TextureUnit {
			consteval int ShadowMaps() {
				return std::to_underlying(Constants::Bindings::Texture::ShadowMaps);
			}

			consteval int TerrainChunkGrid() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainChunkGrid);
			}

			consteval int TerrainMaxHeight() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainMaxHeight);
			}

			consteval int TerrainHeightmap() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainHeightmap);
			}

			consteval int TerrainBiomeMap() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainBiomeMap);
			}

			consteval int Refraction() {
				return std::to_underlying(Constants::Bindings::Texture::Refraction);
			}

			consteval int NoiseSimplex() {
				return std::to_underlying(Constants::Bindings::Texture::NoiseSimplex);
			}

			consteval int NoiseCurl() {
				return std::to_underlying(Constants::Bindings::Texture::NoiseCurl);
			}

			consteval int NoiseBlue() {
				return std::to_underlying(Constants::Bindings::Texture::NoiseBlue);
			}

			consteval int NoiseExtra() {
				return std::to_underlying(Constants::Bindings::Texture::NoiseExtra);
			}

			consteval int NoisePhasor() {
				return std::to_underlying(Constants::Bindings::Texture::NoisePhasor);
			}

			consteval int AtmosphereTransmittance() {
				return std::to_underlying(Constants::Bindings::Texture::AtmosphereTransmittance);
			}

			consteval int AtmosphereMultiScattering() {
				return std::to_underlying(Constants::Bindings::Texture::AtmosphereMultiScattering);
			}

			consteval int AtmosphereSkyView() {
				return std::to_underlying(Constants::Bindings::Texture::AtmosphereSkyView);
			}

			consteval int AtmosphereAerialPerspective() {
				return std::to_underlying(Constants::Bindings::Texture::AtmosphereAerialPerspective);
			}

			consteval int AtmosphereCloudShadow() {
				return std::to_underlying(Constants::Bindings::Texture::AtmosphereCloudShadow);
			}

			consteval int WindData() {
				return std::to_underlying(Constants::Bindings::Texture::WindData);
			}

			consteval int HiZ() {
				return std::to_underlying(Constants::Bindings::Texture::HiZ);
			}

			consteval int TerrainBakedParams() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainBakedParams);
			}

			consteval int TerrainRawHeightmap() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainRawHeightmap);
			}

			consteval int TerrainBiomeImage() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainBiomeImage);
			}

			consteval int TerrainHeightmapImage() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainHeightmapImage);
			}

			consteval int TerrainBakedParamsImage() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainBakedParamsImage);
			}

			consteval int TerrainHorizonMap() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainHorizonMap);
			}

			consteval int TerrainShadowMap() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainShadowMap);
			}

			consteval int TerrainShadowMapImage() {
				return std::to_underlying(Constants::Bindings::Texture::TerrainShadowMapImage);
			}

			consteval int VolumetricLightTexture() {
				return std::to_underlying(Constants::Bindings::Texture::VolumetricLightTexture);
			}
		} // namespace TextureUnit

		namespace SsboBinding {
			consteval int DecorInstances() {
				return std::to_underlying(Constants::Bindings::Ssbo::DecorInstances);
			}

			consteval int AutoExposure() {
				return std::to_underlying(Constants::Bindings::Ssbo::AutoExposure);
			}

			consteval int BoneMatrix() {
				return std::to_underlying(Constants::Bindings::Ssbo::BoneMatrix);
			}

			consteval int OcclusionVisibility() {
				return std::to_underlying(Constants::Bindings::Ssbo::OcclusionVisibility);
			}

			consteval int ParticleGridHeads() {
				return std::to_underlying(Constants::Bindings::Ssbo::ParticleGridHeads);
			}

			consteval int ParticleGridNext() {
				return std::to_underlying(Constants::Bindings::Ssbo::ParticleGridNext);
			}

			consteval int ParticleBuffer() {
				return std::to_underlying(Constants::Bindings::Ssbo::ParticleBuffer);
			}

			consteval int IndirectionBuffer() {
				return std::to_underlying(Constants::Bindings::Ssbo::IndirectionBuffer);
			}

			consteval int TrailPoints() {
				return std::to_underlying(Constants::Bindings::Ssbo::TrailPoints);
			}

			consteval int TrailInstances() {
				return std::to_underlying(Constants::Bindings::Ssbo::TrailInstances);
			}

			consteval int TrailSpineData() {
				return std::to_underlying(Constants::Bindings::Ssbo::TrailSpineData);
			}

			consteval int CommonUniforms() {
				return std::to_underlying(Constants::Bindings::Ssbo::CommonUniforms);
			}

			consteval int EmitterBuffer() {
				return std::to_underlying(Constants::Bindings::Ssbo::EmitterBuffer);
			}

			consteval int TerrainChunkInfo() {
				return std::to_underlying(Constants::Bindings::Ssbo::TerrainChunkInfo);
			}

			consteval int SliceData() {
				return std::to_underlying(Constants::Bindings::Ssbo::SliceData);
			}

			consteval int DecorAllInstances() {
				return std::to_underlying(Constants::Bindings::Ssbo::DecorAllInstances);
			}

			consteval int DecorChunkParams() {
				return std::to_underlying(Constants::Bindings::Ssbo::DecorChunkParams);
			}

			consteval int VisibleParticleIndices() {
				return std::to_underlying(Constants::Bindings::Ssbo::VisibleParticleIndices);
			}

			consteval int ParticleDrawCommand() {
				return std::to_underlying(Constants::Bindings::Ssbo::ParticleDrawCommand);
			}

			consteval int TerrainProbes() {
				return std::to_underlying(Constants::Bindings::Ssbo::TerrainProbes);
			}

			consteval int SdfVolumes() {
				return std::to_underlying(Constants::Bindings::Ssbo::SdfVolumes);
			}

			consteval int LiveParticleIndices() {
				return std::to_underlying(Constants::Bindings::Ssbo::LiveParticleIndices);
			}

			consteval int BehaviorDrawCommand() {
				return std::to_underlying(Constants::Bindings::Ssbo::BehaviorDrawCommand);
			}

			consteval int GrassInstances() {
				return std::to_underlying(Constants::Bindings::Ssbo::GrassInstances);
			}

			consteval int GrassIndirect() {
				return std::to_underlying(Constants::Bindings::Ssbo::GrassIndirect);
			}

			consteval int WeatherGridA() {
				return std::to_underlying(Constants::Bindings::Ssbo::WeatherGridA);
			}

			consteval int WeatherGridB() {
				return std::to_underlying(Constants::Bindings::Ssbo::WeatherGridB);
			}

			consteval int DecorIndirect() {
				return std::to_underlying(Constants::Bindings::Ssbo::DecorIndirect);
			}

			consteval int DecorBlockValidity() {
				return std::to_underlying(Constants::Bindings::Ssbo::DecorBlockValidity);
			}

			consteval int MeshExplosionFragments() {
				return std::to_underlying(Constants::Bindings::Ssbo::MeshExplosionFragments);
			}

			consteval int TrailGeneratedVBO() {
				return std::to_underlying(Constants::Bindings::Ssbo::TrailGeneratedVBO);
			}

			consteval int AtmosphereSH() {
				return std::to_underlying(Constants::Bindings::Ssbo::AtmosphereSH);
			}

			consteval int VolumetricLightAccumulation() {
				return std::to_underlying(Constants::Bindings::Ssbo::VolumetricLightAccumulation);
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
					return 15.0f;
				}

				consteval float FirstPersonEyeHeight() {
					return 4.8f;
				}

				consteval float FirstPersonCrouchHeight() {
					return 1.5f;
				}

				consteval float FirstPersonSprintMultiplier() {
					return 2.0f;
				}

				consteval float FirstPersonJumpForce() {
					return 12.5f;
				}

				consteval float FirstPersonGravity() {
					return 18.0f;
				}

				consteval float FirstPersonGroundSmoothing() {
					return 5.0f;
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
					return 0.15f;
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
					return 2000000;
				}

				consteval size_t MaxIndices() {
					return 4000000;
				}

				consteval size_t StaticVertexLimit() {
					return 1000000;
				}

				consteval size_t StaticIndexLimit() {
					return 2000000;
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
					consteval float GoldR() {
						return 1.0f;
					}

					consteval float GoldG() {
						return 0.84f;
					}

					consteval float GoldB() {
						return 0.0f;
					}

					consteval float SilverR() {
						return 0.75f;
					}

					consteval float SilverG() {
						return 0.75f;
					}

					consteval float SilverB() {
						return 0.75f;
					}

					consteval float BlackR() {
						return 0.01f;
					}

					consteval float BlackG() {
						return 0.01f;
					}

					consteval float BlackB() {
						return 0.01f;
					}

					consteval float BlueR() {
						return 0.0f;
					}

					consteval float BlueG() {
						return 0.5f;
					}

					consteval float BlueB() {
						return 1.0f;
					}

					consteval float NeonGreenR() {
						return 0.2f;
					}

					consteval float NeonGreenG() {
						return 1.0f;
					}

					consteval float NeonGreenB() {
						return 0.2f;
					}

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
