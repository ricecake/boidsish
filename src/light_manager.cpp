#include "light_manager.h"

namespace Boidsish {

	void LightManager::AddLight(const Light& light) {
		_lights.push_back(light);
	}

	std::vector<Light>& LightManager::GetLights() {
		return _lights;
	}

	void LightManager::Update(float deltaTime) {
		// For now, this is a placeholder.
		// In the future, we could update light positions, colors, etc. here.
	}

    glm::vec3 LightManager::GetAmbientLight() const {
        return _ambient_light;
    }

    void LightManager::SetAmbientLight(const glm::vec3& ambient) {
        _ambient_light = ambient;
    }

	std::vector<Light*> LightManager::GetShadowCastingLights() {
		std::vector<Light*> shadow_lights;
		for (auto& light : _lights) {
			if (light.casts_shadow) {
				shadow_lights.push_back(&light);
			}
		}
		return shadow_lights;
	}

	int LightManager::GetShadowCastingLightCount() const {
		int count = 0;
		for (const auto& light : _lights) {
			if (light.casts_shadow) {
				++count;
			}
		}
		return count;
	}

} // namespace Boidsish
