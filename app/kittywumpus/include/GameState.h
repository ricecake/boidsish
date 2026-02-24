#pragma once

#include <memory>

#include <glm/glm.hpp>

namespace Boidsish {

class Visualizer;
class KittywumpusPlane;
struct KittywumpusInputController;

// INTEGRATION_POINT: Add new game states here for future features
// (e.g., MISSION_BRIEFING, INVENTORY, SHOP)
enum class GameState {
	MAIN_MENU,
	FLIGHT_MODE,
	LANDING_TRANSITION,
	FIRST_PERSON_MODE,
	TAKEOFF_TRANSITION,
	GAME_OVER
};

class GameStateManager {
public:
	GameStateManager();

	GameState GetState() const { return state_; }

	void Update(
		float dt,
		Visualizer& viz,
		std::shared_ptr<KittywumpusPlane> plane,
		const KittywumpusInputController& input
	);

	// Called when entering main menu state
	void SetupMainMenu(Visualizer& viz);

	// Called when entering game over state
	void SetupGameOver(Visualizer& viz, int final_score);

	// Get takeoff charge progress (0.0 to 1.0)
	float GetTakeoffChargeProgress() const;

	// Check if any key was pressed to start/restart
	bool WasAnyKeyPressed(const KittywumpusInputController& input) const;

	// Get FPS rig model for rendering (only valid in FPS mode)
	std::shared_ptr<class Model> GetFPSRigModel() const;

private:
	void TransitionTo(GameState new_state, Visualizer& viz, std::shared_ptr<KittywumpusPlane> plane);
	void UpdateFlightMode(float dt, Visualizer& viz, std::shared_ptr<KittywumpusPlane> plane, const KittywumpusInputController& input);
	void UpdateLandingTransition(float dt, Visualizer& viz, std::shared_ptr<KittywumpusPlane> plane);
	void UpdateFirstPersonMode(float dt, Visualizer& viz, std::shared_ptr<KittywumpusPlane> plane, const KittywumpusInputController& input);
	void UpdateTakeoffTransition(float dt, Visualizer& viz, std::shared_ptr<KittywumpusPlane> plane);

	GameState state_ = GameState::MAIN_MENU;
	float transition_time_ = 0.0f;
	float takeoff_charge_ = 0.0f;

	// Camera transition state
	glm::vec3 transition_start_pos_;
	glm::vec3 transition_end_pos_;
	float transition_start_yaw_ = 0.0f;
	float transition_start_pitch_ = 0.0f;

	// HUD message handles for cleanup
	int title_msg_id_ = -1;
	int prompt_msg_id_ = -1;
	int game_over_msg_id_ = -1;
	int score_msg_id_ = -1;
	int restart_msg_id_ = -1;

	// Transition durations
	static constexpr float kLandingTransitionDuration = 1.0f;
	static constexpr float kTakeoffTransitionDuration = 0.5f;
	static constexpr float kTakeoffChargeRequired = 3.0f;
	static constexpr float kLandingHeightThreshold = 2.0f;
};

} // namespace Boidsish
