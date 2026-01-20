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

	private:
		std::vector<Light> _lights{{{0, 50, -500}, 10.0f, {1, 0.5f, 0.25f}}};
	};

} // namespace Boidsish

#endif // LIGHT_MANAGER_H
