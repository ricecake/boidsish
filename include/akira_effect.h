#pragma once

#include <memory>
#include <vector>

#include "constants.h"
#include "terrain_generator_interface.h"
#include <glm/glm.hpp>

class Shader;

namespace Boidsish {

	/**
	 * @brief Represents a single Akira visual effect instance.
	 *
	 * The effect goes through several phases:
	 * 1. GROWING: A small emissive white dot grows to the final deformation radius.
	 * 2. TRIGGERED: The terrain deformation is applied.
	 * 3. FADING: The dot turns metallic silver, gains iridescence, and fades out.
	 */
	struct AkiraEffect {
		enum class Phase { GROWING, FADING, FINISHED };

		glm::vec3 center;
		float     radius;
		float     elapsed_time = 0.0f;
		float     growth_duration = Constants::Class::Akira::DefaultGrowthDuration();
		float     fade_duration = Constants::Class::Akira::DefaultFadeDuration();
		Phase     phase = Phase::GROWING;
		bool      deformation_triggered = false;

		AkiraEffect(const glm::vec3& c, float r): center(c), radius(r) {}

		float GetGrowthProgress() const { return std::min(1.0f, elapsed_time / growth_duration); }

		float GetFadeProgress() const {
			if (phase != Phase::FADING)
				return 0.0f;
			return std::min(1.0f, (elapsed_time - growth_duration) / fade_duration);
		}
	};

	/**
	 * @brief Manages active Akira effects and their rendering.
	 */
	class AkiraEffectManager {
	public:
		AkiraEffectManager();
		~AkiraEffectManager();

		/**
		 * @brief Trigger a new Akira effect.
		 */
		void Trigger(const glm::vec3& position, float radius);

		/**
		 * @brief Update all active effects.
		 */
		void Update(float delta_time, ITerrainGenerator& terrain);

		/**
		 * @brief Render all active effects.
		 */
		void Render(const glm::mat4& view, const glm::mat4& projection, float time);

		/**
		 * @brief Get the shader used for rendering Akira effects.
		 */
		Shader* GetShader() const { return shader_.get(); }

	private:
		std::vector<AkiraEffect> effects_;
		std::unique_ptr<Shader>  shader_;
	};

} // namespace Boidsish
