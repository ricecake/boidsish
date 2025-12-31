#include "fire_effect_manager.h"
#include "shader.h"

namespace Boidsish {

	FireEffectManager::FireEffectManager() {
		// Use the correct ComputeShader class for the .comp file
		m_compute_shader = std::make_shared<ComputeShader>("shaders/fire.comp");
		m_render_shader = std::make_shared<Shader>("shaders/fire.vert", "shaders/fire.frag");
	}

	FireEffectManager::~FireEffectManager() = default;

	std::shared_ptr<FireEffect>
	FireEffectManager::AddEffect(const glm::vec3& position, const glm::vec3& direction, size_t particle_count) {
		auto effect =
			std::make_shared<FireEffect>(position, direction, particle_count, m_compute_shader, m_render_shader);
		m_effects.push_back(effect);
		return effect;
	}

	void FireEffectManager::Update(float time, float delta_time) {
		for (auto& effect : m_effects) {
			effect->Update(time, delta_time);
		}
	}

	void FireEffectManager::Render(const glm::mat4& view, const glm::mat4& projection) {
		for (auto& effect : m_effects) {
			effect->Render(view, projection);
		}
	}

} // namespace Boidsish
