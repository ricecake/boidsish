#pragma once
#include <functional>
#include <optional>
#include <variant>
#include <vector>
#include <string>
#include <glm/glm.hpp>

template <class... Ts>
struct overload: Ts... {
	using Ts::operator()...;
};

namespace Boidsish {
	namespace state {
		struct LbmConstraint {
			std::optional<float> min;
			std::optional<float> max;
			std::optional<float> target;

			bool operator==(const LbmConstraint& other) const = default;
		};

		struct WeatherConstraints {
			LbmConstraint temperature;
			LbmConstraint pressure;
			LbmConstraint humidity;
			LbmConstraint velocity;
			LbmConstraint aerosols;

			bool operator==(const WeatherConstraints& other) const = default;
		};

		struct GrassSettings {
			bool enabled = true;
			float lengthMultiplier = 1.0f;
			float widthMultiplier = 1.0f;
			float densityMultiplier = 1.0f;
			float rigidityMultiplier = 1.0f;
			float windMultiplier = 1.0f;
		};

		struct WeatherSettings {
			bool enabled = true;
			float timeScale = 0.005f;
			float spatialScale = 0.001f;
			float holdThreshold = 0.05f;
			float temperature = 288.15f;
			float precipitation = 0.0f;
			float humidity = 0.5f;
			float windStrength = 0.05f;
			float windSpeed = 0.075f;
			float windFrequency = 0.01f;
			float cloudCoverage = 0.5f;
			bool macroSimEnabled = true;
			float simTau = 0.8f;
			bool strictEnforcement = false;
			float nudgeStiffness = 1.0f;
			WeatherConstraints constraints;
		};

		struct AtmosphereSettings {
			bool enabled = true;
			float hazeDensity = 1.0f;
			float hazeHeight = 20.0f;
			glm::vec3 hazeColor = glm::vec3(0.6f, 0.7f, 0.8f);
			float cloudDensity = 0.2f;
			float cloudAltitude = 400.0f;
			float cloudThickness = 200.0f;
			float cloudCoverage = 0.5f;
			float cloudWarp = 0.0f;
			glm::vec3 cloudColor = glm::vec3(0.95f, 0.95f, 1.0f);
			float cloudSunLightScale = 1.0f;
			float cloudMoonLightScale = 2.0f;
			float cloudPowderScale = 0.125f;
			float cloudPowderMultiplier = 1.0f;
			float cloudPowderLocalScale = 1.0f;
			float cloudShadowOpticalDepthMultiplier = 1.0f;
			float cloudShadowStepMultiplier = 1.0f;
			float cloudBeerPowderMix = 0.6f;
			float rayleighScale = 1.1f;
			float mieScale = 0.35f;
			float mieAnisotropy = 0.8f;
			float multiScatScale = 1.0f;
			float ambientScatScale = 1.0f;
			float atmosphereHeight = 120.0f;
			glm::vec3 rayleighScattering = glm::vec3(5.802f, 13.558f, 33.100f) * 1e-3f;
			float mieScattering = 3.996f * 1e-3f;
			float mieExtinction = 4.440f * 1e-3f;
			glm::vec3 ozoneAbsorption = glm::vec3(0.650f, 1.881f, 0.085f) * 1e-3f;
			float rayleighScaleHeight = 8.0f;
			float mieScaleHeight = 1.2f;
			float colorVarianceScale = 1.0f;
			float colorVarianceStrength = 0.0f;
			float cloudShadowIntensity = 0.5f;
		};

		struct DayNightSettings {
			bool enabled = true;
			float time = 12.0f;
			float speed = 0.1f;
			bool paused = false;
			float lunarAlbedo = 0.07f;
			glm::vec3 moonTint = glm::vec3(1.0f);
			float lunarMonth = 28.0f;
			float moonPhaseDays = 0.0f;
		};

		struct ParticleSettings {
			bool enabled = true;
			float ambientDensity = 0.15f;
			uint32_t limitBirds = 1000;
			uint32_t limitLeaves = 1000;
			uint32_t limitPetals = 1000;
			uint32_t limitBubbles = 1000;
			uint32_t limitFireflies = 1000;
			uint32_t limitSnow = 1000;
			uint32_t limitRain = 1000;
			uint32_t limitDust = 1000;

			// Actual counts
			uint32_t countBirds = 0;
			uint32_t countLeaves = 0;
			uint32_t countPetals = 0;
			uint32_t countBubbles = 0;
			uint32_t countFireflies = 0;
			uint32_t countSnow = 0;
			uint32_t countRain = 0;
			uint32_t countDust = 0;
		};

		struct TerrainSettings {
			bool renderTerrain = true;
			bool renderFloor = false;
			bool forceBoth = false;
			float worldScale = 1.0f;
			bool foliageEnabled = true;
			float foliagePixelThreshold = 10.0f;
		};

		struct VolumetricSettings {
			bool enabled = true;
			float intensity = 1.0f;
			float anisotropy = 0.7f;
			float temporalAlpha = 0.95f;
		};

		struct ErosionSettings {
			bool enabled = true;
			float strength = 0.12f;
			float scale = 0.15f;
			float detail = 1.5f;
			float gullyWeight = 0.5f;
			float maxDist = 450.0f;
		};

		struct BloomLayerSettings {
			bool toneMappingEnabled = true;
			int toneMappingMode = 5;
			bool autoExposureEnabled = true;
			float targetLuminance = 0.25f;
			float minExposure = 0.01f;
			float maxExposure = 25.0f;
			float speedUp = 3.0f;
			float speedDown = 1.0f;
			float centerWeightTightness = 4.0f;
			glm::vec2 focusPoint = glm::vec2(0.5f, 0.5f);
			float histogramLowCutoff = 0.1f;
			float histogramHighCutoff = 0.95f;
			float uchimuraP = 1.0f;
			float uchimuraA = 1.0f;
			float uchimuraM = 0.22f;
			float uchimuraL = 0.4f;
			float uchimuraC = 1.33f;
			float uchimuraB = 0.0f;
			bool autoTuneEnabled = true;
			float minContrast = 0.6f;
			float maxContrast = 1.3f;
			float targetBrightness = 1.0f;
			glm::vec3 cdlSlope = glm::vec3(1.0f);
			glm::vec3 cdlOffset = glm::vec3(0.0f);
			glm::vec3 cdlPower = glm::vec3(1.0f);
			float cdlSaturation = 1.0f;
			float whiteTemp = 6500.0f;
			float whiteTint = 0.0f;
			bool ltmEnabled = true;
			float ltmEvSpread = 2.0f;
			float ltmTarget = 0.5f;
			float ltmSigma = 0.2f;
			float ltmWeightContrast = 0.0f;
			float ltmWeightSaturation = 0.0f;
			float ltmWeightExposedness = 1.0f;
			float ltmBoostLocalContrast = 0.0f;
		};

		struct BloomSettings {
			bool enabled = true;
			float intensity = 0.075f;
			float threshold = 1.0f;
			BloomLayerSettings scene;
			BloomLayerSettings sky;
		};

		struct MoodSettings {
			bool enabled = true;
			bool userOverride = false;
		};

		class SystemConfiguration {
		public:
			GrassSettings grass;
			WeatherSettings weather;
			AtmosphereSettings atmosphere;
			DayNightSettings dayNight;
			ParticleSettings particles;
			TerrainSettings terrain;
			VolumetricSettings volumetric;
			ErosionSettings erosion;
			BloomSettings bloom;
			MoodSettings mood;
		};

		class SystemState {
		public:
			SystemConfiguration target;
			SystemConfiguration actual;
		};

		namespace actions {
			// Grass Actions
			struct SetGrassEnabled { bool value; };
			struct SetGrassLength { float value; };
			struct SetGrassWidth { float value; };
			struct SetGrassDensity { float value; };
			struct SetGrassRigidity { float value; };
			struct SetGrassWind { float value; };

			// Weather Actions
			struct SetWeatherEnabled { bool value; };
			struct SetWeatherTimeScale { float value; };
			struct SetWeatherSpatialScale { float value; };
			struct SetWeatherHoldThreshold { float value; };
			struct SetWeatherTemperature { float value; };
			struct SetWeatherPrecipitation { float value; };
			struct SetWeatherHumidity { float value; };
			struct SetWeatherWindStrength { float value; };
			struct SetWeatherWindSpeed { float value; };
			struct SetWeatherWindFrequency { float value; };
			struct SetWeatherCloudCoverage { float value; };
			struct SetWeatherMacroSimEnabled { bool value; };
			struct SetWeatherSimTau { float value; };
			struct SetWeatherStrictEnforcement { bool value; };
			struct SetWeatherNudgeStiffness { float value; };

			struct SetWeatherLbmConstraint {
				std::string type; // "temperature", "pressure", etc.
				std::optional<float> min;
				std::optional<float> max;
				std::optional<float> target;
			};

			struct InjectPressure { glm::vec3 pos; float pressureHpa; float burstStrength; };
			struct InjectAerosol { glm::vec3 pos; float concentration; };
			struct InjectTemperature { glm::vec3 pos; float temperatureK; };

			// Atmosphere Actions
			struct SetAtmosphereEnabled { bool value; };
			struct SetHazeDensity { float value; };
			struct SetHazeHeight { float value; };
			struct SetHazeColor { glm::vec3 value; };
			struct SetCloudDensity { float value; };
			struct SetCloudAltitude { float value; };
			struct SetCloudThickness { float value; };
			struct SetCloudCoverage { float value; };
			struct SetCloudWarp { float value; };
			struct SetCloudColor { glm::vec3 value; };
			struct SetCloudSunLightScale { float value; };
			struct SetCloudMoonLightScale { float value; };
			struct SetCloudPowderScale { float value; };
			struct SetCloudPowderMultiplier { float value; };
			struct SetCloudPowderLocalScale { float value; };
			struct SetCloudShadowOpticalDepthMultiplier { float value; };
			struct SetCloudShadowStepMultiplier { float value; };
			struct SetCloudBeerPowderMix { float value; };
			struct SetRayleighScale { float value; };
			struct SetMieScale { float value; };
			struct SetMieAnisotropy { float value; };
			struct SetMultiScatScale { float value; };
			struct SetAmbientScatScale { float value; };
			struct SetAtmosphereHeight { float value; };
			struct SetRayleighScattering { glm::vec3 value; };
			struct SetMieScattering { float value; };
			struct SetMieExtinction { float value; };
			struct SetOzoneAbsorption { glm::vec3 value; };
			struct SetRayleighScaleHeight { float value; };
			struct SetMieScaleHeight { float value; };
			struct SetAtmosphereColorVarianceScale { float value; };
			struct SetAtmosphereColorVarianceStrength { float value; };
			struct SetCloudShadowIntensity { float value; };

			// DayNight Actions
			struct SetDayNightEnabled { bool value; };
			struct SetDayNightTime { float value; };
			struct SetDayNightSpeed { float value; };
			struct SetDayNightPaused { bool value; };
			struct SetLunarAlbedo { float value; };
			struct SetMoonTint { glm::vec3 value; };
			struct SetLunarMonth { float value; };
			struct SetMoonPhaseDays { float value; };

			// Particle Actions
			struct SetParticlesEnabled { bool value; };
			struct SetAmbientParticleDensity { float value; };
			struct SetParticleLimit { std::string type; uint32_t limit; };

			// Terrain Actions
			struct SetRenderTerrain { bool value; };
			struct SetRenderFloor { bool value; };
			struct SetForceBoth { bool value; };
			struct SetWorldScale { float value; };
			struct SetFoliageEnabled { bool value; };
			struct SetFoliagePixelThreshold { float value; };

			// Volumetric Actions
			struct SetVolumetricEnabled { bool value; };
			struct SetVolumetricIntensity { float value; };
			struct SetVolumetricAnisotropy { float value; };
			struct SetVolumetricTemporalAlpha { float value; };

			// Erosion Actions
			struct SetErosionEnabled { bool value; };
			struct SetErosionStrength { float value; };
			struct SetErosionScale { float value; };
			struct SetErosionDetail { float value; };
			struct SetErosionGullyWeight { float value; };
			struct SetErosionMaxDist { float value; };

			// Bloom Actions
			struct SetBloomEnabled { bool value; };
			struct SetBloomIntensity { float value; };
			struct SetBloomThreshold { float value; };

			struct SetBloomLayerToneMappingEnabled { bool isSky; bool value; };
			struct SetBloomLayerToneMappingMode { bool isSky; int value; };
			struct SetBloomLayerAutoExposureEnabled { bool isSky; bool value; };
			struct SetBloomLayerTargetLuminance { bool isSky; float value; };
			struct SetBloomLayerExposureLimits { bool isSky; float min; float max; };
			struct SetBloomLayerAdaptationSpeeds { bool isSky; float up; float down; };
			struct SetBloomLayerCenterWeightTightness { bool isSky; float value; };
			struct SetBloomLayerFocusPoint { bool isSky; glm::vec2 value; };
			struct SetBloomLayerHistogramCutoffs { bool isSky; float low; float high; };
			struct SetBloomLayerUchimuraParams { bool isSky; float P; float a; float m; float l; float c; float b; };
			struct SetBloomLayerAutoTuneEnabled { bool isSky; bool value; };
			struct SetBloomLayerAutoTuneConstraints { bool isSky; float minC; float maxC; float targetB; };
			struct SetBloomLayerCdlParams { bool isSky; glm::vec3 slope; glm::vec3 offset; glm::vec3 power; float saturation; };
			struct SetBloomLayerWhiteBalance { bool isSky; float temp; float tint; };
			struct SetBloomLayerLtmEnabled { bool isSky; bool value; };
			struct SetBloomLayerLtmParams { bool isSky; float evSpread; float target; float sigma; };
			struct SetBloomLayerLtmWeights { bool isSky; float contrast; float saturation; float exposedness; };
			struct SetBloomLayerLtmBoostLocalContrast { bool isSky; float value; };

			// Mood Actions
			struct SetMoodEnabled { bool value; };
			struct SetMoodUserOverride { bool value; };

			// Granular Sync Actions
			struct SyncGrassActual { GrassSettings value; };
			struct SyncWeatherActual { WeatherSettings value; };
			struct SyncAtmosphereActual { AtmosphereSettings value; };
			struct SyncDayNightActual { DayNightSettings value; };
			struct SyncParticleActual { ParticleSettings value; };
			struct SyncTerrainActual { TerrainSettings value; };
			struct SyncVolumetricActual { VolumetricSettings value; };
			struct SyncErosionActual { ErosionSettings value; };
			struct SyncBloomActual { BloomSettings value; };
			struct SyncMoodActual { MoodSettings value; };

			struct SyncActual {
				SystemConfiguration actual;
			};
		}; // namespace actions

		using Action = std::variant<
			actions::SetGrassEnabled,
			actions::SetGrassLength,
			actions::SetGrassWidth,
			actions::SetGrassDensity,
			actions::SetGrassRigidity,
			actions::SetGrassWind,
			actions::SetWeatherEnabled,
			actions::SetWeatherTimeScale,
			actions::SetWeatherSpatialScale,
			actions::SetWeatherHoldThreshold,
			actions::SetWeatherTemperature,
			actions::SetWeatherPrecipitation,
			actions::SetWeatherHumidity,
			actions::SetWeatherWindStrength,
			actions::SetWeatherWindSpeed,
			actions::SetWeatherWindFrequency,
			actions::SetWeatherCloudCoverage,
			actions::SetWeatherMacroSimEnabled,
			actions::SetWeatherSimTau,
			actions::SetWeatherStrictEnforcement,
			actions::SetWeatherNudgeStiffness,
			actions::SetWeatherLbmConstraint,
			actions::InjectPressure,
			actions::InjectAerosol,
			actions::InjectTemperature,
			actions::SetAtmosphereEnabled,
			actions::SetHazeDensity,
			actions::SetHazeHeight,
			actions::SetHazeColor,
			actions::SetCloudDensity,
			actions::SetCloudAltitude,
			actions::SetCloudThickness,
			actions::SetCloudCoverage,
			actions::SetCloudWarp,
			actions::SetCloudColor,
			actions::SetCloudSunLightScale,
			actions::SetCloudMoonLightScale,
			actions::SetCloudPowderScale,
			actions::SetCloudPowderMultiplier,
			actions::SetCloudPowderLocalScale,
			actions::SetCloudShadowOpticalDepthMultiplier,
			actions::SetCloudShadowStepMultiplier,
			actions::SetCloudBeerPowderMix,
			actions::SetRayleighScale,
			actions::SetMieScale,
			actions::SetMieAnisotropy,
			actions::SetMultiScatScale,
			actions::SetAmbientScatScale,
			actions::SetAtmosphereHeight,
			actions::SetRayleighScattering,
			actions::SetMieScattering,
			actions::SetMieExtinction,
			actions::SetOzoneAbsorption,
			actions::SetRayleighScaleHeight,
			actions::SetMieScaleHeight,
			actions::SetAtmosphereColorVarianceScale,
			actions::SetAtmosphereColorVarianceStrength,
			actions::SetCloudShadowIntensity,
			actions::SetDayNightEnabled,
			actions::SetDayNightTime,
			actions::SetDayNightSpeed,
			actions::SetDayNightPaused,
			actions::SetLunarAlbedo,
			actions::SetMoonTint,
			actions::SetLunarMonth,
			actions::SetMoonPhaseDays,
			actions::SetParticlesEnabled,
			actions::SetAmbientParticleDensity,
			actions::SetParticleLimit,
			actions::SetRenderTerrain,
			actions::SetRenderFloor,
			actions::SetForceBoth,
			actions::SetWorldScale,
			actions::SetFoliageEnabled,
			actions::SetFoliagePixelThreshold,
			actions::SetVolumetricEnabled,
			actions::SetVolumetricIntensity,
			actions::SetVolumetricAnisotropy,
			actions::SetVolumetricTemporalAlpha,
			actions::SetErosionEnabled,
			actions::SetErosionStrength,
			actions::SetErosionScale,
			actions::SetErosionDetail,
			actions::SetErosionGullyWeight,
			actions::SetErosionMaxDist,
			actions::SetBloomEnabled,
			actions::SetBloomIntensity,
			actions::SetBloomThreshold,
			actions::SetBloomLayerToneMappingEnabled,
			actions::SetBloomLayerToneMappingMode,
			actions::SetBloomLayerAutoExposureEnabled,
			actions::SetBloomLayerTargetLuminance,
			actions::SetBloomLayerExposureLimits,
			actions::SetBloomLayerAdaptationSpeeds,
			actions::SetBloomLayerCenterWeightTightness,
			actions::SetBloomLayerFocusPoint,
			actions::SetBloomLayerHistogramCutoffs,
			actions::SetBloomLayerUchimuraParams,
			actions::SetBloomLayerAutoTuneEnabled,
			actions::SetBloomLayerAutoTuneConstraints,
			actions::SetBloomLayerCdlParams,
			actions::SetBloomLayerWhiteBalance,
			actions::SetBloomLayerLtmEnabled,
			actions::SetBloomLayerLtmParams,
			actions::SetBloomLayerLtmWeights,
			actions::SetBloomLayerLtmBoostLocalContrast,
			actions::SetMoodEnabled,
			actions::SetMoodUserOverride,
			actions::SyncGrassActual,
			actions::SyncWeatherActual,
			actions::SyncAtmosphereActual,
			actions::SyncDayNightActual,
			actions::SyncParticleActual,
			actions::SyncTerrainActual,
			actions::SyncVolumetricActual,
			actions::SyncErosionActual,
			actions::SyncBloomActual,
			actions::SyncMoodActual,
			actions::SyncActual
		>;

		// Reducer must remain deterministic and completely side-effect free
		inline SystemState AppReducer(const SystemState& previousState, const Action& action) {
			SystemState newState = previousState; // Deep copy state snapshot

			auto getBloomLayer = [&](bool isSky) -> BloomLayerSettings& {
				return isSky ? newState.target.bloom.sky : newState.target.bloom.scene;
			};

			std::visit(overload {
				[&](actions::SetGrassEnabled a) { newState.target.grass.enabled = a.value; },
				[&](actions::SetGrassLength a) { newState.target.grass.lengthMultiplier = a.value; },
				[&](actions::SetGrassWidth a) { newState.target.grass.widthMultiplier = a.value; },
				[&](actions::SetGrassDensity a) { newState.target.grass.densityMultiplier = a.value; },
				[&](actions::SetGrassRigidity a) { newState.target.grass.rigidityMultiplier = a.value; },
				[&](actions::SetGrassWind a) { newState.target.grass.windMultiplier = a.value; },
				[&](actions::SetWeatherEnabled a) { newState.target.weather.enabled = a.value; },
				[&](actions::SetWeatherTimeScale a) { newState.target.weather.timeScale = a.value; },
				[&](actions::SetWeatherSpatialScale a) { newState.target.weather.spatialScale = a.value; },
				[&](actions::SetWeatherHoldThreshold a) { newState.target.weather.holdThreshold = a.value; },
				[&](actions::SetWeatherTemperature a) { newState.target.weather.temperature = a.value; },
				[&](actions::SetWeatherPrecipitation a) { newState.target.weather.precipitation = a.value; },
				[&](actions::SetWeatherHumidity a) { newState.target.weather.humidity = a.value; },
				[&](actions::SetWeatherWindStrength a) { newState.target.weather.windStrength = a.value; },
				[&](actions::SetWeatherWindSpeed a) { newState.target.weather.windSpeed = a.value; },
				[&](actions::SetWeatherWindFrequency a) { newState.target.weather.windFrequency = a.value; },
				[&](actions::SetWeatherCloudCoverage a) { newState.target.weather.cloudCoverage = a.value; },
				[&](actions::SetWeatherMacroSimEnabled a) { newState.target.weather.macroSimEnabled = a.value; },
				[&](actions::SetWeatherSimTau a) { newState.target.weather.simTau = a.value; },
				[&](actions::SetWeatherStrictEnforcement a) { newState.target.weather.strictEnforcement = a.value; },
				[&](actions::SetWeatherNudgeStiffness a) { newState.target.weather.nudgeStiffness = a.value; },
				[&](actions::SetWeatherLbmConstraint a) {
					LbmConstraint* c = nullptr;
					if (a.type == "temperature") c = &newState.target.weather.constraints.temperature;
					else if (a.type == "pressure") c = &newState.target.weather.constraints.pressure;
					else if (a.type == "humidity") c = &newState.target.weather.constraints.humidity;
					else if (a.type == "velocity") c = &newState.target.weather.constraints.velocity;
					else if (a.type == "aerosols") c = &newState.target.weather.constraints.aerosols;
					if (c) {
						c->min = a.min;
						c->max = a.max;
						c->target = a.target;
					}
				},
				[&](actions::InjectPressure a) {},
				[&](actions::InjectAerosol a) {},
				[&](actions::InjectTemperature a) {},
				[&](actions::SetAtmosphereEnabled a) { newState.target.atmosphere.enabled = a.value; },
				[&](actions::SetHazeDensity a) { newState.target.atmosphere.hazeDensity = a.value; },
				[&](actions::SetHazeHeight a) { newState.target.atmosphere.hazeHeight = a.value; },
				[&](actions::SetHazeColor a) { newState.target.atmosphere.hazeColor = a.value; },
				[&](actions::SetCloudDensity a) { newState.target.atmosphere.cloudDensity = a.value; },
				[&](actions::SetCloudAltitude a) { newState.target.atmosphere.cloudAltitude = a.value; },
				[&](actions::SetCloudThickness a) { newState.target.atmosphere.cloudThickness = a.value; },
				[&](actions::SetCloudCoverage a) { newState.target.atmosphere.cloudCoverage = a.value; },
				[&](actions::SetCloudWarp a) { newState.target.atmosphere.cloudWarp = a.value; },
				[&](actions::SetCloudColor a) { newState.target.atmosphere.cloudColor = a.value; },
				[&](actions::SetCloudSunLightScale a) { newState.target.atmosphere.cloudSunLightScale = a.value; },
				[&](actions::SetCloudMoonLightScale a) { newState.target.atmosphere.cloudMoonLightScale = a.value; },
				[&](actions::SetCloudPowderScale a) { newState.target.atmosphere.cloudPowderScale = a.value; },
				[&](actions::SetCloudPowderMultiplier a) { newState.target.atmosphere.cloudPowderMultiplier = a.value; },
				[&](actions::SetCloudPowderLocalScale a) { newState.target.atmosphere.cloudPowderLocalScale = a.value; },
				[&](actions::SetCloudShadowOpticalDepthMultiplier a) { newState.target.atmosphere.cloudShadowOpticalDepthMultiplier = a.value; },
				[&](actions::SetCloudShadowStepMultiplier a) { newState.target.atmosphere.cloudShadowStepMultiplier = a.value; },
				[&](actions::SetCloudBeerPowderMix a) { newState.target.atmosphere.cloudBeerPowderMix = a.value; },
				[&](actions::SetRayleighScale a) { newState.target.atmosphere.rayleighScale = a.value; },
				[&](actions::SetMieScale a) { newState.target.atmosphere.mieScale = a.value; },
				[&](actions::SetMieAnisotropy a) { newState.target.atmosphere.mieAnisotropy = a.value; },
				[&](actions::SetMultiScatScale a) { newState.target.atmosphere.multiScatScale = a.value; },
				[&](actions::SetAmbientScatScale a) { newState.target.atmosphere.ambientScatScale = a.value; },
				[&](actions::SetAtmosphereHeight a) { newState.target.atmosphere.atmosphereHeight = a.value; },
				[&](actions::SetRayleighScattering a) { newState.target.atmosphere.rayleighScattering = a.value; },
				[&](actions::SetMieScattering a) { newState.target.atmosphere.mieScattering = a.value; },
				[&](actions::SetMieExtinction a) { newState.target.atmosphere.mieExtinction = a.value; },
				[&](actions::SetOzoneAbsorption a) { newState.target.atmosphere.ozoneAbsorption = a.value; },
				[&](actions::SetRayleighScaleHeight a) { newState.target.atmosphere.rayleighScaleHeight = a.value; },
				[&](actions::SetMieScaleHeight a) { newState.target.atmosphere.mieScaleHeight = a.value; },
				[&](actions::SetAtmosphereColorVarianceScale a) { newState.target.atmosphere.colorVarianceScale = a.value; },
				[&](actions::SetAtmosphereColorVarianceStrength a) { newState.target.atmosphere.colorVarianceStrength = a.value; },
				[&](actions::SetCloudShadowIntensity a) { newState.target.atmosphere.cloudShadowIntensity = a.value; },
				[&](actions::SetDayNightEnabled a) { newState.target.dayNight.enabled = a.value; },
				[&](actions::SetDayNightTime a) { newState.target.dayNight.time = a.value; },
				[&](actions::SetDayNightSpeed a) { newState.target.dayNight.speed = a.value; },
				[&](actions::SetDayNightPaused a) { newState.target.dayNight.paused = a.value; },
				[&](actions::SetLunarAlbedo a) { newState.target.dayNight.lunarAlbedo = a.value; },
				[&](actions::SetMoonTint a) { newState.target.dayNight.moonTint = a.value; },
				[&](actions::SetLunarMonth a) { newState.target.dayNight.lunarMonth = a.value; },
				[&](actions::SetMoonPhaseDays a) { newState.target.dayNight.moonPhaseDays = a.value; },
				[&](actions::SetParticlesEnabled a) { newState.target.particles.enabled = a.value; },
				[&](actions::SetAmbientParticleDensity a) { newState.target.particles.ambientDensity = a.value; },
				[&](actions::SetParticleLimit a) {
					if (a.type == "birds") newState.target.particles.limitBirds = a.limit;
					else if (a.type == "leaves") newState.target.particles.limitLeaves = a.limit;
					else if (a.type == "petals") newState.target.particles.limitPetals = a.limit;
					else if (a.type == "bubbles") newState.target.particles.limitBubbles = a.limit;
					else if (a.type == "fireflies") newState.target.particles.limitFireflies = a.limit;
					else if (a.type == "snow") newState.target.particles.limitSnow = a.limit;
					else if (a.type == "rain") newState.target.particles.limitRain = a.limit;
					else if (a.type == "dust") newState.target.particles.limitDust = a.limit;
				},
				[&](actions::SetRenderTerrain a) { newState.target.terrain.renderTerrain = a.value; },
				[&](actions::SetRenderFloor a) { newState.target.terrain.renderFloor = a.value; },
				[&](actions::SetForceBoth a) { newState.target.terrain.forceBoth = a.value; },
				[&](actions::SetWorldScale a) { newState.target.terrain.worldScale = a.value; },
				[&](actions::SetFoliageEnabled a) { newState.target.terrain.foliageEnabled = a.value; },
				[&](actions::SetFoliagePixelThreshold a) { newState.target.terrain.foliagePixelThreshold = a.value; },
				[&](actions::SetVolumetricEnabled a) { newState.target.volumetric.enabled = a.value; },
				[&](actions::SetVolumetricIntensity a) { newState.target.volumetric.intensity = a.value; },
				[&](actions::SetVolumetricAnisotropy a) { newState.target.volumetric.anisotropy = a.value; },
				[&](actions::SetVolumetricTemporalAlpha a) { newState.target.volumetric.temporalAlpha = a.value; },
				[&](actions::SetErosionEnabled a) { newState.target.erosion.enabled = a.value; },
				[&](actions::SetErosionStrength a) { newState.target.erosion.strength = a.value; },
				[&](actions::SetErosionScale a) { newState.target.erosion.scale = a.value; },
				[&](actions::SetErosionDetail a) { newState.target.erosion.detail = a.value; },
				[&](actions::SetErosionGullyWeight a) { newState.target.erosion.gullyWeight = a.value; },
				[&](actions::SetErosionMaxDist a) { newState.target.erosion.maxDist = a.value; },
				[&](actions::SetBloomEnabled a) { newState.target.bloom.enabled = a.value; },
				[&](actions::SetBloomIntensity a) { newState.target.bloom.intensity = a.value; },
				[&](actions::SetBloomThreshold a) { newState.target.bloom.threshold = a.value; },
				[&](actions::SetBloomLayerToneMappingEnabled a) { getBloomLayer(a.isSky).toneMappingEnabled = a.value; },
				[&](actions::SetBloomLayerToneMappingMode a) { getBloomLayer(a.isSky).toneMappingMode = a.value; },
				[&](actions::SetBloomLayerAutoExposureEnabled a) { getBloomLayer(a.isSky).autoExposureEnabled = a.value; },
				[&](actions::SetBloomLayerTargetLuminance a) { getBloomLayer(a.isSky).targetLuminance = a.value; },
				[&](actions::SetBloomLayerExposureLimits a) { getBloomLayer(a.isSky).minExposure = a.min; getBloomLayer(a.isSky).maxExposure = a.max; },
				[&](actions::SetBloomLayerAdaptationSpeeds a) { getBloomLayer(a.isSky).speedUp = a.up; getBloomLayer(a.isSky).speedDown = a.down; },
				[&](actions::SetBloomLayerCenterWeightTightness a) { getBloomLayer(a.isSky).centerWeightTightness = a.value; },
				[&](actions::SetBloomLayerFocusPoint a) { getBloomLayer(a.isSky).focusPoint = a.value; },
				[&](actions::SetBloomLayerHistogramCutoffs a) { getBloomLayer(a.isSky).histogramLowCutoff = a.low; getBloomLayer(a.isSky).histogramHighCutoff = a.high; },
				[&](actions::SetBloomLayerUchimuraParams a) {
					auto& l = getBloomLayer(a.isSky);
					l.uchimuraP = a.P; l.uchimuraA = a.a; l.uchimuraM = a.m;
					l.uchimuraL = a.l; l.uchimuraC = a.c; l.uchimuraB = a.b;
				},
				[&](actions::SetBloomLayerAutoTuneEnabled a) { getBloomLayer(a.isSky).autoTuneEnabled = a.value; },
				[&](actions::SetBloomLayerAutoTuneConstraints a) {
					auto& l = getBloomLayer(a.isSky);
					l.minContrast = a.minC; l.maxContrast = a.maxC; l.targetBrightness = a.targetB;
				},
				[&](actions::SetBloomLayerCdlParams a) {
					auto& l = getBloomLayer(a.isSky);
					l.cdlSlope = a.slope; l.cdlOffset = a.offset; l.cdlPower = a.power; l.cdlSaturation = a.saturation;
				},
				[&](actions::SetBloomLayerWhiteBalance a) { getBloomLayer(a.isSky).whiteTemp = a.temp; getBloomLayer(a.isSky).whiteTint = a.tint; },
				[&](actions::SetBloomLayerLtmEnabled a) { getBloomLayer(a.isSky).ltmEnabled = a.value; },
				[&](actions::SetBloomLayerLtmParams a) {
					auto& l = getBloomLayer(a.isSky);
					l.ltmEvSpread = a.evSpread; l.ltmTarget = a.target; l.ltmSigma = a.sigma;
				},
				[&](actions::SetBloomLayerLtmWeights a) {
					auto& l = getBloomLayer(a.isSky);
					l.ltmWeightContrast = a.contrast; l.ltmWeightSaturation = a.saturation; l.ltmWeightExposedness = a.exposedness;
				},
				[&](actions::SetBloomLayerLtmBoostLocalContrast a) { getBloomLayer(a.isSky).ltmBoostLocalContrast = a.value; },

				[&](actions::SetMoodEnabled a) { newState.target.mood.enabled = a.value; },
				[&](actions::SetMoodUserOverride a) { newState.target.mood.userOverride = a.value; },
				[&](actions::SyncGrassActual a) { newState.actual.grass = a.value; },
				[&](actions::SyncWeatherActual a) { newState.actual.weather = a.value; },
				[&](actions::SyncAtmosphereActual a) { newState.actual.atmosphere = a.value; },
				[&](actions::SyncDayNightActual a) { newState.actual.dayNight = a.value; },
				[&](actions::SyncParticleActual a) { newState.actual.particles = a.value; },
				[&](actions::SyncTerrainActual a) { newState.actual.terrain = a.value; },
				[&](actions::SyncVolumetricActual a) { newState.actual.volumetric = a.value; },
				[&](actions::SyncErosionActual a) { newState.actual.erosion = a.value; },
				[&](actions::SyncBloomActual a) { newState.actual.bloom = a.value; },
				[&](actions::SyncMoodActual a) { newState.actual.mood = a.value; },
				[&](actions::SyncActual a) { newState.actual = a.actual; }
			}, action);

			return newState;
		}

		class Store {
		public:
			using Reducer = std::function<SystemState(const SystemState&, const Action&)>;
			using Listener = std::function<void(const SystemState&)>;
			using ActionListener = std::function<void(const Action&)>;

			Store(Reducer reducer, SystemState initialState): m_reducer(reducer), m_state(initialState) {}

			// Dispatch an action to trigger state mutations
			void Dispatch(const Action& action) {
				m_state = m_reducer(m_state, action);
				for (const auto& listener : m_listeners) {
					listener(m_state);
				}
				for (const auto& listener : m_actionListeners) {
					listener(action);
				}
			}

			// Read-only access to state
			const SystemState& GetState() const { return m_state; }

			// Register UI reactive hooks
			void Subscribe(Listener listener) { m_listeners.push_back(listener); }

			// Register hooks for transient actions
			void SubscribeAction(ActionListener listener) { m_actionListeners.push_back(listener); }

		private:
			Reducer               m_reducer;
			SystemState           m_state;
			std::vector<Listener> m_listeners;
			std::vector<ActionListener> m_actionListeners;
		};

		void SyncBloomState(void* effect);
	}; // namespace state
}; // namespace Boidsish
