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
			float rayleighScale = 1.1f;
			float mieScale = 0.35f;
			float mieAnisotropy = 0.8f;
			float multiScatScale = 0.0f;
			float ambientScatScale = 0.0f;
			float atmosphereHeight = 120.0f;
			glm::vec3 rayleighScattering = glm::vec3(5.802f, 13.558f, 33.100f) * 1e-3f;
			float mieScattering = 3.996f * 1e-3f;
			float mieExtinction = 4.440f * 1e-3f;
			glm::vec3 ozoneAbsorption = glm::vec3(0.650f, 1.881f, 0.085f) * 1e-3f;
			float rayleighScaleHeight = 8.0f;
			float mieScaleHeight = 1.2f;
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

			// Mood Actions
			struct SetMoodEnabled { bool value; };
			struct SetMoodUserOverride { bool value; };

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
			actions::SetMoodEnabled,
			actions::SetMoodUserOverride,
			actions::SyncActual
		>;

		// Reducer must remain deterministic and completely side-effect free
		inline SystemState AppReducer(const SystemState& previousState, const Action& action) {
			SystemState newState = previousState; // Deep copy state snapshot

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
				[&](actions::SetMoodEnabled a) { newState.target.mood.enabled = a.value; },
				[&](actions::SetMoodUserOverride a) { newState.target.mood.userOverride = a.value; },
				[&](actions::SyncActual a) { newState.actual = a.actual; }
			}, action);

			return newState;
		}

		class Store {
		public:
			using Reducer = std::function<SystemState(const SystemState&, const Action&)>;
			using Listener = std::function<void(const SystemState&)>;

			Store(Reducer reducer, SystemState initialState): m_reducer(reducer), m_state(initialState) {}

			// Dispatch an action to trigger state mutations
			void Dispatch(const Action& action) {
				m_state = m_reducer(m_state, action);
				for (const auto& listener : m_listeners) {
					listener(m_state);
				}
			}

			// Read-only access to state
			const SystemState& GetState() const { return m_state; }

			// Register UI reactive hooks
			void Subscribe(Listener listener) { m_listeners.push_back(listener); }

		private:
			Reducer               m_reducer;
			SystemState           m_state;
			std::vector<Listener> m_listeners;
		};
	}; // namespace state
}; // namespace Boidsish
