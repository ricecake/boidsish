#pragma once

#include <memory>
#include <vector>

#include "fire_effect.h"
#include <glm/glm.hpp>

class Shader;
class ComputeShader; // Forward declare ComputeShader

namespace Boidsish {

	class FireEffectManager {
	public:
		FireEffectManager();
		~FireEffectManager();

		// Non-copyable
		FireEffectManager(const FireEffectManager&) = delete;
		FireEffectManager& operator=(const FireEffectManager&) = delete;

		std::shared_ptr<FireEffect>
		AddEffect(const glm::vec3& position, const glm::vec3& direction, size_t particle_count = 8192);
		void RemoveEffect(const std::shared_ptr<FireEffect>& effect);

		void Update(float time, float delta_time);
		void Render(const glm::mat4& view, const glm::mat4& projection);

	private:
		std::vector<std::shared_ptr<FireEffect>> m_effects;

		// Correctly typed compute shader pointer
		std::shared_ptr<ComputeShader> m_compute_shader;
		std::shared_ptr<Shader>        m_render_shader;
	};

} // namespace Boidsish
