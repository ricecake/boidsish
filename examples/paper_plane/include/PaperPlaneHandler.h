#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <set>

#include "spatial_entity_handler.h"

namespace Boidsish {

	class GuidedMissileLauncher;
	class Terrain; // Forward declaration
	extern int selected_weapon;

	class PaperPlane; // Forward declaration
	class HudGauge;
	class HudScore;

	class PaperPlaneHandler: public SpatialEntityHandler {
	public:
		PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool);
		void PreTimestep(float time, float delta_time) override;
		void RemoveEntity(int id) override;

		void SetHealthGauge(std::shared_ptr<HudGauge> gauge) { health_gauge_ = gauge; }

		void SetScoreIndicator(std::shared_ptr<HudScore> indicator) { score_indicator_ = indicator; }

		int GetScore() const;

		void AddScore(int delta, const std::string& label) const;

		void SetStreakIndicator(std::shared_ptr<HudNumber> indicator) { streak_indicator_ = indicator; }

		void OnPlaneDeath(int score) const;

		/**
		 * @brief Finds a suitable starting position and orientation for the paper plane.
		 * Searches the nearby terrain for a point at high altitude with a downslope/flat gradient.
		 */
		void PreparePlane(std::shared_ptr<PaperPlane> plane);

		void RecordTarget(std::shared_ptr<EntityBase> target) const;
		int  GetTargetCount(std::shared_ptr<EntityBase> target) const;

	private:
		std::optional<glm::vec3>
		FindOccludedSpawnPosition(const glm::vec3& player_pos, const glm::vec3& player_forward);

		mutable std::mutex                    target_mutex_;
		mutable std::map<int, int>            target_counts_;
		std::map<std::pair<int, int>, int>    spawned_launchers_;
		std::random_device                    rd_;
		std::mt19937                          eng_;
		float                                 damage_timer_ = 0.0f;
		float                                 enemy_spawn_timer_ = 5.0f;
		std::uniform_real_distribution<float> damage_dist_;
		std::map<std::pair<int, int>, float>  launcher_cooldowns_;
		std::shared_ptr<HudGauge>             health_gauge_;
		std::shared_ptr<HudScore>             score_indicator_;
		std::shared_ptr<HudNumber>            streak_indicator_;
		int                                   streak_ = 0;
		int                                   last_collected_sequence_id_ = -1;
	};

} // namespace Boidsish
