#include "light_manager.h"

namespace Boidsish {

	LightManager::LightManager() {
		_lights.push_back(Light{{0, 50, -500}, 10.0f, {1, 0.5f, 0.25f}, 1, glm::mat4(1.0f)});
	}

	void LightManager::AddLight(const Light& light) {
		_lights.push_back(light);
	}

	std::vector<Light>& LightManager::GetLights() {
		return _lights;
	}

	int LightManager::GetShadowCasterIndex() const {
		for (int i = 0; i < _lights.size(); ++i) {
			if (_lights[i].casts_shadow) {
				return i;
			}
		}
		return -1; // No shadow caster found
	}

	void LightManager::Update(float deltaTime) {
		// For now, this is a placeholder.
		// In the future, we could update light positions, colors, etc. here.
	}

} // namespace Boidsish
