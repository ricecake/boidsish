#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "state.h"

namespace Boidsish {

struct MoodSettings; // the rich one from mood_manager.h with optionals

namespace state {

	struct GenerationCounters {
		uint32_t grass = 0;
		uint32_t weather = 0;
		uint32_t atmosphere = 0;
		uint32_t dayNight = 0;
		uint32_t particles = 0;
		uint32_t terrain = 0;
		uint32_t volumetric = 0;
		uint32_t erosion = 0;
		uint32_t bloom = 0;
		uint32_t mood = 0;
	};

	struct SimulationReport {
		struct Weather {
			float temperature = 288.15f;
			float precipitation = 0.0f;
			float humidity = 0.5f;
			float windStrength = 0.05f;
			float windSpeed = 0.075f;
			float windFrequency = 0.01f;
			float cloudCoverage = 0.5f;
		} weather;

		struct Atmosphere {
			float hazeDensity = 1.0f;
			float hazeHeight = 20.0f;
			glm::vec3 hazeColor = glm::vec3(0.6f, 0.7f, 0.8f);
			float cloudDensity = 0.2f;
			float cloudAltitude = 400.0f;
			float cloudThickness = 200.0f;
			float cloudCoverage = 0.5f;
			glm::vec3 cloudColor = glm::vec3(0.95f, 0.95f, 1.0f);
			float rayleighScale = 1.1f;
			float mieScale = 0.35f;
		} atmosphere;

		struct DayNight {
			float time = 12.0f;
			float sunAngle = 0.0f;
			float moonAngle = 0.0f;
			float moonPhaseDays = 0.0f;
		} dayNight;

		struct Particles {
			float ambientDensity = 0.0f;
			uint32_t countBirds = 0, limitBirds = 0;
			uint32_t countLeaves = 0, limitLeaves = 0;
			uint32_t countPetals = 0, limitPetals = 0;
			uint32_t countBubbles = 0, limitBubbles = 0;
			uint32_t countFireflies = 0, limitFireflies = 0;
			uint32_t countFairies = 0, limitFairies = 0;
			uint32_t countSnow = 0, limitSnow = 0;
			uint32_t countRain = 0, limitRain = 0;
			uint32_t countDust = 0, limitDust = 0;
		} particles;

		GenerationCounters gen;
	};

	struct TransientCommands {
		struct PressureInjection { glm::vec3 pos; float pressureHpa; float burstStrength; };
		struct AerosolInjection { glm::vec3 pos; float concentration; };
		struct TemperatureInjection { glm::vec3 pos; float temperatureK; };

		std::vector<PressureInjection> pressure;
		std::vector<AerosolInjection> aerosol;
		std::vector<TemperatureInjection> temperature;

		void Clear() {
			pressure.clear();
			aerosol.clear();
			temperature.clear();
		}
		bool Empty() const { return pressure.empty() && aerosol.empty() && temperature.empty(); }
	};

	struct StateFrame {
		SystemConfiguration* user_input = nullptr;
		SystemConfiguration* effective = nullptr;
		SimulationReport simulation;
		TransientCommands commands;
		GenerationCounters gen;
	};

	// FrameBuffer: double-buffered state with typed Apply() dispatch.
	// Actions are stack-allocated structs — Apply is a direct in-place mutation + gen bump.
	class FrameBuffer {
	public:
		FrameBuffer();
		~FrameBuffer();

		void Swap();

		const StateFrame& Read() const { return frames_[read_idx_]; }
		StateFrame& Write() { return frames_[write_idx_]; }

		SystemConfiguration& UserInput() { return *frames_[write_idx_].user_input; }
		const SystemConfiguration& UserInput() const { return *frames_[write_idx_].user_input; }
		const SystemConfiguration& Effective() const { return *frames_[read_idx_].effective; }
		const SimulationReport& Simulation() const { return frames_[read_idx_].simulation; }
		const GenerationCounters& Generations() const { return frames_[read_idx_].gen; }

		// --- Grass mutations ---
		void Apply(const actions::SetGrassEnabled& a) { UserInput().grass.enabled = a.value; BumpGrass(); }
		void Apply(const actions::SetGrassLength& a) { UserInput().grass.lengthMultiplier = a.value; BumpGrass(); }
		void Apply(const actions::SetGrassWidth& a) { UserInput().grass.widthMultiplier = a.value; BumpGrass(); }
		void Apply(const actions::SetGrassDensity& a) { UserInput().grass.densityMultiplier = a.value; BumpGrass(); }
		void Apply(const actions::SetGrassRigidity& a) { UserInput().grass.rigidityMultiplier = a.value; BumpGrass(); }
		void Apply(const actions::SetGrassWind& a) { UserInput().grass.windMultiplier = a.value; BumpGrass(); }
		void Apply(const actions::SetGrassLodScaleFactor& a) { UserInput().grass.lodScaleFactor = a.value; BumpGrass(); }
		void Apply(const actions::SetGrassLodBaseRange& a) { UserInput().grass.lodBaseRange = a.value; BumpGrass(); }
		void Apply(const actions::SetGrassBaseScale& a) { UserInput().grass.baseScale = a.value; BumpGrass(); }

		// --- Weather mutations ---
		void Apply(const actions::SetWeatherEnabled& a) { UserInput().weather.enabled = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherTimeScale& a) { UserInput().weather.timeScale = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherSpatialScale& a) { UserInput().weather.spatialScale = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherHoldThreshold& a) { UserInput().weather.holdThreshold = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherTemperature& a) { UserInput().weather.temperature = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherPrecipitation& a) { UserInput().weather.precipitation = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherHumidity& a) { UserInput().weather.humidity = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherWindStrength& a) { UserInput().weather.windStrength = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherWindSpeed& a) { UserInput().weather.windSpeed = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherWindFrequency& a) { UserInput().weather.windFrequency = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherCloudCoverage& a) { UserInput().weather.cloudCoverage = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherMacroSimEnabled& a) { UserInput().weather.macroSimEnabled = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherSimTau& a) { UserInput().weather.simTau = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherStrictEnforcement& a) { UserInput().weather.strictEnforcement = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherNudgeStiffness& a) { UserInput().weather.nudgeStiffness = a.value; BumpWeather(); }
		void Apply(const actions::SetWeatherLbmConstraint& a) {
			LbmConstraint* c = nullptr;
			if (a.type == "temperature") c = &UserInput().weather.constraints.temperature;
			else if (a.type == "pressure") c = &UserInput().weather.constraints.pressure;
			else if (a.type == "humidity") c = &UserInput().weather.constraints.humidity;
			else if (a.type == "velocity") c = &UserInput().weather.constraints.velocity;
			else if (a.type == "aerosols") c = &UserInput().weather.constraints.aerosols;
			if (c) { c->min = a.min; c->max = a.max; c->target = a.target; }
			BumpWeather();
		}

		// --- Weather transient commands (events, not state) ---
		void Apply(const actions::InjectPressure& a) { Write().commands.pressure.push_back({a.pos, a.pressureHpa, a.burstStrength}); }
		void Apply(const actions::InjectAerosol& a) { Write().commands.aerosol.push_back({a.pos, a.concentration}); }
		void Apply(const actions::InjectTemperature& a) { Write().commands.temperature.push_back({a.pos, a.temperatureK}); }

		// --- Weather attribute constraints ---
		void Apply(const actions::SetWeatherAttrConstraint& a) {
			auto* c = GetAttrConstraint(a.attr);
			if (c) { *c = a.constraint; BumpWeather(); }
		}
		void Apply(const actions::ClearWeatherAttrConstraint& a) {
			auto* c = GetAttrConstraint(a.attr);
			if (c) { c->clear(); BumpWeather(); }
		}
		void Apply(const actions::ClearAllWeatherAttrConstraints&) {
			UserInput().weather.attrConstraints.clearAll();
			BumpWeather();
		}

		// --- Atmosphere mutations ---
		void Apply(const actions::SetAtmosphereEnabled& a) { UserInput().atmosphere.enabled = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetHazeDensity& a) { UserInput().atmosphere.hazeDensity = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetHazeHeight& a) { UserInput().atmosphere.hazeHeight = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetHazeColor& a) { UserInput().atmosphere.hazeColor = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudDensity& a) { UserInput().atmosphere.cloudDensity = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudAltitude& a) { UserInput().atmosphere.cloudAltitude = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudThickness& a) { UserInput().atmosphere.cloudThickness = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudCoverage& a) { UserInput().atmosphere.cloudCoverage = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudWarp& a) { UserInput().atmosphere.cloudWarp = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudColor& a) { UserInput().atmosphere.cloudColor = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudSunLightScale& a) { UserInput().atmosphere.cloudSunLightScale = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudMoonLightScale& a) { UserInput().atmosphere.cloudMoonLightScale = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudPowderScale& a) { UserInput().atmosphere.cloudPowderScale = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudPowderMultiplier& a) { UserInput().atmosphere.cloudPowderMultiplier = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudPowderLocalScale& a) { UserInput().atmosphere.cloudPowderLocalScale = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudShadowOpticalDepthMultiplier& a) { UserInput().atmosphere.cloudShadowOpticalDepthMultiplier = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudShadowStepMultiplier& a) { UserInput().atmosphere.cloudShadowStepMultiplier = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudBeerPowderMix& a) { UserInput().atmosphere.cloudBeerPowderMix = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetRayleighScale& a) { UserInput().atmosphere.rayleighScale = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetMieScale& a) { UserInput().atmosphere.mieScale = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetMieAnisotropy& a) { UserInput().atmosphere.mieAnisotropy = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetMultiScatScale& a) { UserInput().atmosphere.multiScatScale = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetAmbientScatScale& a) { UserInput().atmosphere.ambientScatScale = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetAtmosphereHeight& a) { UserInput().atmosphere.atmosphereHeight = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetRayleighScattering& a) { UserInput().atmosphere.rayleighScattering = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetMieScattering& a) { UserInput().atmosphere.mieScattering = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetMieExtinction& a) { UserInput().atmosphere.mieExtinction = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetOzoneAbsorption& a) { UserInput().atmosphere.ozoneAbsorption = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetRayleighScaleHeight& a) { UserInput().atmosphere.rayleighScaleHeight = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetMieScaleHeight& a) { UserInput().atmosphere.mieScaleHeight = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetAtmosphereColorVarianceScale& a) { UserInput().atmosphere.colorVarianceScale = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetAtmosphereColorVarianceStrength& a) { UserInput().atmosphere.colorVarianceStrength = a.value; BumpAtmosphere(); }
		void Apply(const actions::SetCloudShadowIntensity& a) { UserInput().atmosphere.cloudShadowIntensity = a.value; BumpAtmosphere(); }

		// --- DayNight mutations ---
		void Apply(const actions::SetDayNightEnabled& a) { UserInput().dayNight.enabled = a.value; BumpDayNight(); }
		void Apply(const actions::SetDayNightTime& a) { UserInput().dayNight.time = a.value; BumpDayNight(); }
		void Apply(const actions::SetDayNightSpeed& a) { UserInput().dayNight.speed = a.value; BumpDayNight(); }
		void Apply(const actions::SetDayNightPaused& a) { UserInput().dayNight.paused = a.value; BumpDayNight(); }
		void Apply(const actions::SetLunarAlbedo& a) { UserInput().dayNight.lunarAlbedo = a.value; BumpDayNight(); }
		void Apply(const actions::SetMoonTint& a) { UserInput().dayNight.moonTint = a.value; BumpDayNight(); }
		void Apply(const actions::SetLunarMonth& a) { UserInput().dayNight.lunarMonth = a.value; BumpDayNight(); }
		void Apply(const actions::SetMoonPhaseDays& a) { UserInput().dayNight.moonPhaseDays = a.value; BumpDayNight(); }

		// --- Particle mutations ---
		void Apply(const actions::SetParticlesEnabled& a) { UserInput().particles.enabled = a.value; BumpParticles(); }
		void Apply(const actions::SetAmbientParticleDensity& a) { UserInput().particles.ambientDensity = a.value; BumpParticles(); }
		void Apply(const actions::SetParticleRatio& a) {
			auto& p = UserInput().particles;
			if (a.type == "birds") p.ratioBirds = a.ratio;
			else if (a.type == "leaves") p.ratioLeaves = a.ratio;
			else if (a.type == "petals") p.ratioPetals = a.ratio;
			else if (a.type == "bubbles") p.ratioBubbles = a.ratio;
			else if (a.type == "fireflies") p.ratioFireflies = a.ratio;
			else if (a.type == "fairies") p.ratioFairies = a.ratio;
			else if (a.type == "snow") p.ratioSnow = a.ratio;
			else if (a.type == "rain") p.ratioRain = a.ratio;
			else if (a.type == "dust") p.ratioDust = a.ratio;
			BumpParticles();
		}

		// --- Terrain mutations ---
		void Apply(const actions::SetRenderTerrain& a) { UserInput().terrain.renderTerrain = a.value; BumpTerrain(); }
		void Apply(const actions::SetRenderFloor& a) { UserInput().terrain.renderFloor = a.value; BumpTerrain(); }
		void Apply(const actions::SetForceBoth& a) { UserInput().terrain.forceBoth = a.value; BumpTerrain(); }
		void Apply(const actions::SetWorldScale& a) { UserInput().terrain.worldScale = a.value; BumpTerrain(); }
		void Apply(const actions::SetFoliageEnabled& a) { UserInput().terrain.foliageEnabled = a.value; BumpTerrain(); }
		void Apply(const actions::SetFoliagePixelThreshold& a) { UserInput().terrain.foliagePixelThreshold = a.value; BumpTerrain(); }

		// --- Volumetric mutations ---
		void Apply(const actions::SetVolumetricEnabled& a) { UserInput().volumetric.enabled = a.value; BumpVolumetric(); }
		void Apply(const actions::SetVolumetricIntensity& a) { UserInput().volumetric.intensity = a.value; BumpVolumetric(); }
		void Apply(const actions::SetVolumetricAnisotropy& a) { UserInput().volumetric.anisotropy = a.value; BumpVolumetric(); }
		void Apply(const actions::SetVolumetricTemporalAlpha& a) { UserInput().volumetric.temporalAlpha = a.value; BumpVolumetric(); }

		// --- Erosion mutations ---
		void Apply(const actions::SetErosionEnabled& a) { UserInput().erosion.enabled = a.value; BumpErosion(); }
		void Apply(const actions::SetErosionStrength& a) { UserInput().erosion.strength = a.value; BumpErosion(); }
		void Apply(const actions::SetErosionScale& a) { UserInput().erosion.scale = a.value; BumpErosion(); }
		void Apply(const actions::SetErosionDetail& a) { UserInput().erosion.detail = a.value; BumpErosion(); }
		void Apply(const actions::SetErosionGullyWeight& a) { UserInput().erosion.gullyWeight = a.value; BumpErosion(); }
		void Apply(const actions::SetErosionMaxDist& a) { UserInput().erosion.maxDist = a.value; BumpErosion(); }

		// --- Bloom mutations ---
		void Apply(const actions::SetBloomEnabled& a) { UserInput().bloom.enabled = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomIntensity& a) { UserInput().bloom.intensity = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomThreshold& a) { UserInput().bloom.threshold = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomLayerToneMappingEnabled& a) { GetBloomLayer(a.isSky).toneMappingEnabled = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomLayerToneMappingMode& a) { GetBloomLayer(a.isSky).toneMappingMode = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomLayerAutoExposureEnabled& a) { GetBloomLayer(a.isSky).autoExposureEnabled = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomLayerTargetLuminance& a) { GetBloomLayer(a.isSky).targetLuminance = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomLayerExposureLimits& a) { auto& l = GetBloomLayer(a.isSky); l.minExposure = a.min; l.maxExposure = a.max; BumpBloom(); }
		void Apply(const actions::SetBloomLayerAdaptationSpeeds& a) { auto& l = GetBloomLayer(a.isSky); l.speedUp = a.up; l.speedDown = a.down; BumpBloom(); }
		void Apply(const actions::SetBloomLayerCenterWeightTightness& a) { GetBloomLayer(a.isSky).centerWeightTightness = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomLayerFocusPoint& a) { GetBloomLayer(a.isSky).focusPoint = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomLayerHistogramCutoffs& a) { auto& l = GetBloomLayer(a.isSky); l.histogramLowCutoff = a.low; l.histogramHighCutoff = a.high; BumpBloom(); }
		void Apply(const actions::SetBloomLayerUchimuraParams& a) {
			auto& l = GetBloomLayer(a.isSky);
			l.uchimuraP = a.P; l.uchimuraA = a.a; l.uchimuraM = a.m;
			l.uchimuraL = a.l; l.uchimuraC = a.c; l.uchimuraB = a.b;
			BumpBloom();
		}
		void Apply(const actions::SetBloomLayerAutoTuneEnabled& a) { GetBloomLayer(a.isSky).autoTuneEnabled = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomLayerAutoTuneConstraints& a) {
			auto& l = GetBloomLayer(a.isSky);
			l.minContrast = a.minC; l.maxContrast = a.maxC; l.targetBrightness = a.targetB;
			BumpBloom();
		}
		void Apply(const actions::SetBloomLayerCdlParams& a) {
			auto& l = GetBloomLayer(a.isSky);
			l.cdlSlope = a.slope; l.cdlOffset = a.offset; l.cdlPower = a.power; l.cdlSaturation = a.saturation;
			BumpBloom();
		}
		void Apply(const actions::SetBloomLayerWhiteBalance& a) { auto& l = GetBloomLayer(a.isSky); l.whiteTemp = a.temp; l.whiteTint = a.tint; BumpBloom(); }
		void Apply(const actions::SetBloomLayerLtmEnabled& a) { GetBloomLayer(a.isSky).ltmEnabled = a.value; BumpBloom(); }
		void Apply(const actions::SetBloomLayerLtmParams& a) {
			auto& l = GetBloomLayer(a.isSky);
			l.ltmEvSpread = a.evSpread; l.ltmTarget = a.target; l.ltmSigma = a.sigma;
			BumpBloom();
		}
		void Apply(const actions::SetBloomLayerLtmWeights& a) {
			auto& l = GetBloomLayer(a.isSky);
			l.ltmWeightContrast = a.contrast; l.ltmWeightSaturation = a.saturation; l.ltmWeightExposedness = a.exposedness;
			BumpBloom();
		}
		void Apply(const actions::SetBloomLayerLtmBoostLocalContrast& a) { GetBloomLayer(a.isSky).ltmBoostLocalContrast = a.value; BumpBloom(); }

		// --- Mood mutations ---
		void Apply(const actions::SetMoodEnabled& a) { UserInput().mood.enabled = a.value; BumpMood(); }
		void Apply(const actions::SetMoodUserOverride& a) { UserInput().mood.userOverride = a.value; BumpMood(); }

	private:
		void BumpGrass() { frames_[write_idx_].gen.grass++; }
		void BumpWeather() { frames_[write_idx_].gen.weather++; }
		void BumpAtmosphere() { frames_[write_idx_].gen.atmosphere++; }
		void BumpDayNight() { frames_[write_idx_].gen.dayNight++; }
		void BumpParticles() { frames_[write_idx_].gen.particles++; }
		void BumpTerrain() { frames_[write_idx_].gen.terrain++; }
		void BumpVolumetric() { frames_[write_idx_].gen.volumetric++; }
		void BumpErosion() { frames_[write_idx_].gen.erosion++; }
		void BumpBloom() { frames_[write_idx_].gen.bloom++; }
		void BumpMood() { frames_[write_idx_].gen.mood++; }

		BloomLayerSettings& GetBloomLayer(bool isSky) {
			return isSky ? UserInput().bloom.sky : UserInput().bloom.scene;
		}

		AttributeConstraint* GetAttrConstraint(const std::string& attr) {
			auto& ac = UserInput().weather.attrConstraints;
			if (attr == "temperature") return &ac.temperature;
			if (attr == "precipitation") return &ac.precipitation;
			if (attr == "humidity") return &ac.humidity;
			if (attr == "windStrength") return &ac.windStrength;
			if (attr == "windSpeed") return &ac.windSpeed;
			if (attr == "windFrequency") return &ac.windFrequency;
			if (attr == "cloudCoverage") return &ac.cloudCoverage;
			if (attr == "hazeDensity") return &ac.hazeDensity;
			if (attr == "hazeHeight") return &ac.hazeHeight;
			if (attr == "cloudDensity") return &ac.cloudDensity;
			if (attr == "cloudAltitude") return &ac.cloudAltitude;
			if (attr == "cloudThickness") return &ac.cloudThickness;
			if (attr == "rayleighScale") return &ac.rayleighScale;
			if (attr == "mieScale") return &ac.mieScale;
			return nullptr;
		}

		StateFrame frames_[2];
		int read_idx_ = 0;
		int write_idx_ = 1;
	};

	// Merge user_input + mood overlay → effective, bumping generation counters where changed.
	void MergeEffective(FrameBuffer& fb, const ::Boidsish::MoodSettings& mood);

} // namespace state
} // namespace Boidsish
