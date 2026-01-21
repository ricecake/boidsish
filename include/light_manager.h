#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include <vector>

#include "light.h"

namespace Boidsish {

	class LightManager {
	public:
		LightManager();
		void                AddLight(const Light& light);
		std::vector<Light>& GetLights();
		int                 GetShadowCasterIndex() const;
		void                Update(float deltaTime);

	private:
		std::vector<Light> _lights;
	};

} // namespace Boidsish

#endif // LIGHT_MANAGER_H
