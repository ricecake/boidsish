#pragma once
#include <optional>
#include <string>
#include <glm/glm.hpp>

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
			float lodScaleFactor = 2.0f;
			float lodBaseRange = 30.0f;
			float baseScale = 0.25f;

			bool operator==(const GrassSettings& other) const = default;
		};

		struct AttributeConstraint {
			std::optional<float> target;
			std::optional<float> min;
			std::optional<float> max;

			bool operator==(const AttributeConstraint& other) const = default;
			bool empty() const { return !target && !min && !max; }
			void clear() { target = std::nullopt; min = std::nullopt; max = std::nullopt; }
		};

		struct WeatherAttributeConstraints {
			AttributeConstraint temperature;
			AttributeConstraint precipitation;
			AttributeConstraint humidity;
			AttributeConstraint windStrength;
			AttributeConstraint windSpeed;
			AttributeConstraint windFrequency;
			AttributeConstraint cloudCoverage;
			AttributeConstraint hazeDensity;
			AttributeConstraint hazeHeight;
			AttributeConstraint cloudDensity;
			AttributeConstraint cloudAltitude;
			AttributeConstraint cloudThickness;
			AttributeConstraint rayleighScale;
			AttributeConstraint mieScale;

			void clearAll() {
				temperature.clear(); precipitation.clear(); humidity.clear();
				windStrength.clear(); windSpeed.clear(); windFrequency.clear();
				cloudCoverage.clear(); hazeDensity.clear(); hazeHeight.clear();
				cloudDensity.clear(); cloudAltitude.clear(); cloudThickness.clear();
				rayleighScale.clear(); mieScale.clear();
			}

			bool operator==(const WeatherAttributeConstraints& other) const = default;
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
			WeatherAttributeConstraints attrConstraints;

			bool operator==(const WeatherSettings& other) const = default;
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

			bool operator==(const AtmosphereSettings& other) const = default;
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

			bool operator==(const DayNightSettings& other) const = default;
		};

		struct ParticleSettings {
			bool enabled = true;
			float ambientDensity = 0.15f;

			float ratioBirds = 0.05f;
			float ratioLeaves = 0.25f;
			float ratioPetals = 0.25f;
			float ratioBubbles = 0.15f;
			float ratioFireflies = 0.25f;
			float ratioFairies = 0.1f;
			float ratioSnow = 0.5f;
			float ratioRain = 0.5f;
			float ratioDust = 0.25f;

			bool operator==(const ParticleSettings& other) const = default;
		};

		struct TerrainSettings {
			bool renderTerrain = true;
			bool renderFloor = false;
			bool forceBoth = false;
			float worldScale = 1.0f;
			bool foliageEnabled = true;
			float foliagePixelThreshold = 10.0f;

			bool operator==(const TerrainSettings& other) const = default;
		};

		struct VolumetricSettings {
			bool enabled = true;
			float intensity = 1.0f;
			float anisotropy = 0.7f;
			float temporalAlpha = 0.95f;

			bool operator==(const VolumetricSettings& other) const = default;
		};

		struct ErosionSettings {
			bool enabled = true;
			float strength = 0.12f;
			float scale = 0.15f;
			float detail = 1.5f;
			float gullyWeight = 0.5f;
			float maxDist = 450.0f;

			bool operator==(const ErosionSettings& other) const = default;
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

			bool operator==(const BloomLayerSettings& other) const = default;
		};

		struct BloomSettings {
			bool enabled = true;
			float intensity = 0.075f;
			float threshold = 1.0f;
			BloomLayerSettings scene;
			BloomLayerSettings sky;

			bool operator==(const BloomSettings& other) const = default;
		};

		struct MoodSettings {
			bool enabled = true;
			bool userOverride = false;

			bool operator==(const MoodSettings& other) const = default;
		};

		struct SystemConfiguration {
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

			bool operator==(const SystemConfiguration& other) const = default;
		};

		namespace actions {
			// Grass
			struct SetGrassEnabled { bool value; };
			struct SetGrassLength { float value; };
			struct SetGrassWidth { float value; };
			struct SetGrassDensity { float value; };
			struct SetGrassRigidity { float value; };
			struct SetGrassWind { float value; };

			struct SetGrassLodScaleFactor { float value; };
			struct SetGrassLodBaseRange { float value; };
			struct SetGrassBaseScale { float value; };

			// Weather
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
				std::string type;
				std::optional<float> min;
				std::optional<float> max;
				std::optional<float> target;
			};

			struct InjectPressure { glm::vec3 pos; float pressureHpa; float burstStrength; };
			struct InjectAerosol { glm::vec3 pos; float concentration; };
			struct InjectTemperature { glm::vec3 pos; float temperatureK; };

			// Weather attribute constraints (target/min/max per attribute)
			struct SetWeatherAttrConstraint { std::string attr; AttributeConstraint constraint; };
			struct ClearWeatherAttrConstraint { std::string attr; };
			struct ClearAllWeatherAttrConstraints {};

			// Atmosphere
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

			// DayNight
			struct SetDayNightEnabled { bool value; };
			struct SetDayNightTime { float value; };
			struct SetDayNightSpeed { float value; };
			struct SetDayNightPaused { bool value; };
			struct SetLunarAlbedo { float value; };
			struct SetMoonTint { glm::vec3 value; };
			struct SetLunarMonth { float value; };
			struct SetMoonPhaseDays { float value; };

			// Particles
			struct SetParticlesEnabled { bool value; };
			struct SetAmbientParticleDensity { float value; };
			struct SetParticleRatio { std::string type; float ratio; };

			// Terrain
			struct SetRenderTerrain { bool value; };
			struct SetRenderFloor { bool value; };
			struct SetForceBoth { bool value; };
			struct SetWorldScale { float value; };
			struct SetFoliageEnabled { bool value; };
			struct SetFoliagePixelThreshold { float value; };

			// Volumetric
			struct SetVolumetricEnabled { bool value; };
			struct SetVolumetricIntensity { float value; };
			struct SetVolumetricAnisotropy { float value; };
			struct SetVolumetricTemporalAlpha { float value; };

			// Erosion
			struct SetErosionEnabled { bool value; };
			struct SetErosionStrength { float value; };
			struct SetErosionScale { float value; };
			struct SetErosionDetail { float value; };
			struct SetErosionGullyWeight { float value; };
			struct SetErosionMaxDist { float value; };

			// Bloom
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

			// Mood
			struct SetMoodEnabled { bool value; };
			struct SetMoodUserOverride { bool value; };
		}; // namespace actions
	}; // namespace state
}; // namespace Boidsish
