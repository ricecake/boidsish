#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include <vector>

#include "constants.h"
#include "light.h"

namespace Boidsish {

	class LightManager {
	public:
		struct DayNightCycle {
			bool  enabled = true;
			float time = 12.0f;   // 0.0 - 24.0 (12.0 is noon)
			float speed = 0.125f; // Rate of time passage
			bool  paused = false;
			float night_factor = 0.0f; // 0.0 (day) to 1.0 (night)
			float moon_offset = 12.0f; // Hours offset from sun
		};

		int                 AddLight(const Light& light);
		void                RemoveLight(int id);
		Light*              GetLight(int id);
		std::vector<Light>& GetLights();
		void                Update(float deltaTime);
		glm::vec3           GetAmbientLight() const;
		void                SetAmbientLight(const glm::vec3& ambient);

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
		glm::vec3          _ambient_light = Constants::General::Colors::DefaultAmbient();
		DayNightCycle      _cycle;
		int                _next_light_id = 1;
		/*
		    ambient: 53/58/44
		    def: 231/27/0 @0,100,-100->0,-5.7,7.5 and 6.3 intense
		*/
	};
} // namespace Boidsish

#endif // LIGHT_MANAGER_H
