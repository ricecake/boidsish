#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include <vector>

#include "constants.h"
#include "light.h"

namespace Boidsish {

	class ITerrainGenerator;

	class LightManager {
	public:
		struct DayNightCycle {
			bool  enabled = true;
			float time = 12.0f;    // 0.0 - 24.0 (12.0 is noon)
			float speed = 0.0125f; // Rate of time passage
			bool  paused = false;
			float night_factor = 0.0f; // 0.0 (day) to 1.0 (night)
			float moon_offset = 12.0f; // Hours offset from sun (base, before phase drift)
			float moon_azimuth = 70.0f;

			// Lunar phase cycle — the moon's offset drifts over a ~29.5 day period
			// creating the full → half → new → half → full cycle
			float                  moon_phase_days = 0.0f;
			float                  lunar_albedo = 0.005f;
			static constexpr float kLunarMonth = 29.53f;
		};

		int                 AddLight(const Light& light);
		void                RemoveLight(int id);
		Light*              GetLight(int id);
		std::vector<Light>& GetLights();
		void                Update(float deltaTime, ITerrainGenerator* terrain = nullptr, const glm::vec3& cameraPos = glm::vec3(0.0f));
		glm::vec3           GetAmbientLight() const;
		void                SetAmbientLight(const glm::vec3& ambient);

		const AmbientProbe* GetProbes() const { return _probes; }

		DayNightCycle& GetDayNightCycle() { return _cycle; }

		const DayNightCycle& GetDayNightCycle() const { return _cycle; }

		/**
		 * @brief Get lights that cast shadows.
		 * @return Vector of pointers to shadow-casting lights
		 */
		std::vector<Light*> GetShadowCastingLights();

		/**
		 * @brief Get number of shadow-casting lights.
		 */
		int GetShadowCastingLightCount() const;

	private:
		// Default lights cast shadows
		// Sun: Initial azimuth 0 (North), elevation 45 degrees
		// Moon: Initial azimuth 180 (South), elevation -45 degrees
		std::vector<Light> _lights{
			Light::CreateDirectional(0.0f, 45.0f, 1.0f, {1.0f, 0.50196f, 0.25098f}, true),
			Light::CreateDirectional(180.0f, -45.0f, 0.1f, {0.8f, 0.9f, 1.0f}, true)
		};
		glm::vec3     _ambient_light = Constants::General::Colors::DefaultAmbient();
		AmbientProbe  _probes[5];
		DayNightCycle _cycle;
		int           _next_light_id = 1;
		/*
		    ambient: 53/58/44
		    def: 231/27/0 @0,100,-100->0,-5.7,7.5 and 6.3 intense
		*/
	};
} // namespace Boidsish

#endif // LIGHT_MANAGER_H
