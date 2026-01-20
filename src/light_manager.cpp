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

} // namespace Boidsish
