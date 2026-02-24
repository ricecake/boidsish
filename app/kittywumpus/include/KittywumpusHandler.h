#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <set>

#include "spatial_entity_handler.h"

namespace Boidsish {

class GuidedMissileLauncher;
class Terrain;
extern int kittywumpus_selected_weapon;

class KittywumpusPlane;
class HudGauge;
class HudScore;
class HudMessage;

// INTEGRATION_POINT: Add forward declarations for future enemy/objective types
// class GroundEnemy;
// class Objective;
// class MissionManager;

class KittywumpusHandler: public SpatialEntityHandler {
public:
	KittywumpusHandler(task_thread_pool::task_thread_pool& thread_pool);
	void PreTimestep(float time, float delta_time) override;
	void RemoveEntity(int id) override;

	void SetHealthGauge(std::shared_ptr<HudGauge> gauge) { health_gauge_ = gauge; }
	void SetScoreIndicator(std::shared_ptr<HudScore> indicator) { score_indicator_ = indicator; }
	void SetStreakIndicator(std::shared_ptr<HudNumber> indicator) { streak_indicator_ = indicator; }

	int GetScore() const;
	void AddScore(int delta, const std::string& label) const;
	void OnPlaneDeath(int score) const;
	void ClearGameOverHUD();

	void PreparePlane(std::shared_ptr<KittywumpusPlane> plane);

	void RecordTarget(std::shared_ptr<EntityBase> target) const;
	int  GetTargetCount(std::shared_ptr<EntityBase> target) const;

	// Game state awareness
	void SetGameStateFlying(bool is_flying) { is_flying_ = is_flying; }
	bool IsFlying() const { return is_flying_; }
	void SetInMainMenu(bool in_menu) { in_main_menu_ = in_menu; }
	bool IsInMainMenu() const { return in_main_menu_; }

	// HUD management for mode transitions
	void ShowFlightHUD();
	void HideFlightHUD();
	void ShowFPSHUD();
	void HideFPSHUD();

	// INTEGRATION_POINT: Future feature methods
	// void SpawnGroundEnemies(const glm::vec3& player_pos);
	// void UpdateObjectives(float delta_time);
	// void SetCurrentMission(std::shared_ptr<Mission> mission);

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

	// Game state tracking
	bool is_flying_ = true;
	bool in_main_menu_ = true;

	// HUD element references for show/hide
	std::shared_ptr<HudMessage> crosshair_msg_;

	// Game over HUD messages (tracked for cleanup)
	mutable std::shared_ptr<HudMessage> game_over_msg_;
	mutable std::shared_ptr<HudMessage> score_msg_;
	mutable std::shared_ptr<HudMessage> restart_msg_;
};

} // namespace Boidsish
