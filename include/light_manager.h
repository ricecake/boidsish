#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include <vector>

#include "light.h"

namespace Boidsish {

	class LightManager {
	public:
		void                AddLight(const Light& light);
		std::vector<Light>& GetLights();
		void                Update(float deltaTime);
		glm::vec3           GetAmbientLight() const;
		void                SetAmbientLight(const glm::vec3& ambient);

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
		// Default light casts shadows
		std::vector<Light> _lights{Light::CreateDirectional({0, 100, -500}, {0, -1, 1}, 5.0f, {1, 0.5f, 0.25f}, true)};
		glm::vec3          _ambient_light = glm::vec3(0.1f);
	};

} // namespace Boidsish

#endif // LIGHT_MANAGER_H
