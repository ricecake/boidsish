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
		std::vector<Light> _lights{Light::Create({0, 200, 200}, 1.0f, {1, 0.5f, 0.25f}, true)};
	};

} // namespace Boidsish

#endif // LIGHT_MANAGER_H
