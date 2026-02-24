#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "FirstPersonController.h"
#include "GameState.h"
#include "KittywumpusHandler.h"
#include "KittywumpusInputController.h"
#include "KittywumpusPlane.h"
#include "SteeringProbeEntity.h"
#include "constants.h"
#include "decor_manager.h"
#include "graphics.h"
#include "hud.h"
#include "model.h"
#include "steering_probe.h"
#include "terrain_generator_interface.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

// Game state manager instance
static GameStateManager g_game_state;

// HUD elements
static std::shared_ptr<HudMessage> g_title_msg;
static std::shared_ptr<HudMessage> g_prompt_msg;
static std::shared_ptr<HudMessage> g_crosshair_msg;
static std::shared_ptr<HudIconSet> g_weapon_selector;
static std::shared_ptr<HudGauge> g_health_gauge;
static std::shared_ptr<HudScore> g_score_indicator;
static std::shared_ptr<HudNumber> g_streak_indicator;
static std::shared_ptr<HudNumber> g_takeoff_charge_indicator;

// Global plane reference for game state transitions
static std::shared_ptr<KittywumpusPlane> g_plane;
static std::shared_ptr<KittywumpusHandler> g_handler;

// Main menu camera animation state
static float g_menu_camera_time = 0.0f;
static const float kMenuCameraRadius = 300.0f;
static const float kMenuCameraHeight = 150.0f;
static const float kMenuCameraSpeed = 0.1f; // radians per second

void UpdateMainMenuCamera(Visualizer& viz, float delta_time) {
	// Slow circular pan above terrain
	g_menu_camera_time += delta_time * kMenuCameraSpeed;

	auto& cam = viz.GetCamera();
	cam.x = sin(g_menu_camera_time) * kMenuCameraRadius;
	cam.z = cos(g_menu_camera_time) * kMenuCameraRadius;

	// Get terrain height at camera position and stay above it
	auto [terrain_height, terrain_normal] = viz.GetTerrainPropertiesAtPoint(cam.x, cam.z);
	(void)terrain_normal;
	cam.y = std::max(terrain_height + kMenuCameraHeight, kMenuCameraHeight);

	// Look toward center
	cam.yaw = glm::degrees(-g_menu_camera_time) + 180.0f;
	cam.pitch = -10.0f;
}

void SetupMainMenuHUD(Visualizer& viz) {
	// Clear any existing messages
	if (g_title_msg) {
		g_title_msg->SetVisible(true);
	} else {
		g_title_msg = viz.AddHudMessage("KITTYWUMPUS", HudAlignment::MIDDLE_CENTER, {0, -50}, 4.0f);
	}

	if (g_prompt_msg) {
		g_prompt_msg->SetVisible(true);
	} else {
		g_prompt_msg = viz.AddHudMessage("Press any key to begin", HudAlignment::MIDDLE_CENTER, {0, 50}, 1.5f);
	}

	// Hide game HUD elements
	if (g_health_gauge) g_health_gauge->SetVisible(false);
	if (g_score_indicator) g_score_indicator->SetVisible(false);
	if (g_streak_indicator) g_streak_indicator->SetVisible(false);
	if (g_weapon_selector) g_weapon_selector->SetVisible(false);
	if (g_crosshair_msg) g_crosshair_msg->SetVisible(false);
	if (g_takeoff_charge_indicator) g_takeoff_charge_indicator->SetVisible(false);
}

void SetupFlightHUD(Visualizer& viz) {
	(void)viz;
	// Hide menu messages
	if (g_title_msg) g_title_msg->SetVisible(false);
	if (g_prompt_msg) g_prompt_msg->SetVisible(false);

	// Show flight HUD
	if (g_health_gauge) g_health_gauge->SetVisible(true);
	if (g_score_indicator) g_score_indicator->SetVisible(true);
	if (g_streak_indicator) g_streak_indicator->SetVisible(true);
	if (g_weapon_selector) g_weapon_selector->SetVisible(true);

	// Hide FPS elements
	if (g_crosshair_msg) g_crosshair_msg->SetVisible(false);
	if (g_takeoff_charge_indicator) g_takeoff_charge_indicator->SetVisible(false);
}

void SetupFPSHUD(Visualizer& viz) {
	// Hide menu messages
	if (g_title_msg) g_title_msg->SetVisible(false);
	if (g_prompt_msg) g_prompt_msg->SetVisible(false);

	// Hide flight HUD elements
	if (g_health_gauge) g_health_gauge->SetVisible(false);
	if (g_weapon_selector) g_weapon_selector->SetVisible(false);

	// Keep score visible
	if (g_score_indicator) g_score_indicator->SetVisible(true);
	if (g_streak_indicator) g_streak_indicator->SetVisible(false);

	// Show FPS HUD
	if (g_crosshair_msg) {
		g_crosshair_msg->SetVisible(true);
	} else {
		g_crosshair_msg = viz.AddHudMessage("+", HudAlignment::MIDDLE_CENTER, {0, 0}, 1.5f);
	}

	if (g_takeoff_charge_indicator) {
		g_takeoff_charge_indicator->SetVisible(true);
	} else {
		g_takeoff_charge_indicator = viz.AddHudNumber(0.0f, "Takeoff", HudAlignment::BOTTOM_CENTER, {0, -80}, 1);
	}
}

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(
			Constants::Project::Window::DefaultWidth(),
			Constants::Project::Window::DefaultHeight(),
			"Kittywumpus"
		);

		auto terrain = visualizer->GetTerrain();
		terrain->SetWorldScale(2.0f);

		auto decor = visualizer->GetDecorManager();

		DecorProperties teapot_props;
		teapot_props.min_height = 0.01;
		teapot_props.max_height = 95.0f;
		teapot_props.min_density = 0.1f;
		teapot_props.max_density = 0.11f;
		teapot_props.base_scale = 0.008f;
		teapot_props.scale_variance = 0.01f;
		teapot_props.align_to_terrain = true;
		decor->AddDecorType("assets/tree01.obj", teapot_props);

		// Initialize weapon selector
		std::vector<std::string> weaponIcons =
			{"assets/missile-icon.png", "assets/bomb-icon.png", "assets/bullet-icon.png", "assets/icon.png"};
		g_weapon_selector = visualizer->AddHudIconSet(weaponIcons, HudAlignment::TOP_LEFT, {10, 10}, {64, 64}, 10.0f);
		g_weapon_selector->SetSelectedIndex(kittywumpus_selected_weapon);

		// Create handler
		g_handler = std::make_shared<KittywumpusHandler>(visualizer->GetThreadPool());
		g_handler->SetVisualizer(visualizer);

		// Create plane
		auto id = g_handler->AddEntity<KittywumpusPlane>();
		g_plane = std::dynamic_pointer_cast<KittywumpusPlane>(g_handler->GetEntity(id));

		// Prepare starting position
		g_handler->PreparePlane(g_plane);

		visualizer->AddShapeHandler(std::ref(*g_handler));

		// Shape handler for FPS rig model (rendered in FPS mode)
		visualizer->AddShapeHandler([](float) {
			std::vector<std::shared_ptr<Shape>> shapes;
			auto fps_model = g_game_state.GetFPSRigModel();
			if (fps_model) {
				shapes.push_back(fps_model);
			}
			return shapes;
		});

		// Initialize HUD elements
		g_health_gauge = visualizer->AddHudGauge(100.0f, "Health", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20});
		g_handler->SetHealthGauge(g_health_gauge);

		visualizer->AddHudCompass();
		g_score_indicator = visualizer->AddHudScore();
		g_handler->SetScoreIndicator(g_score_indicator);
		g_streak_indicator = visualizer->AddHudNumber(0.0f, "Streak", HudAlignment::TOP_RIGHT, {-160, 50}, 0);
		g_handler->SetStreakIndicator(g_streak_indicator);
		visualizer->AddHudLocation();

		// Create input controller
		auto controller = std::make_shared<KittywumpusInputController>();
		g_plane->SetController(controller);

		// Track previous game state for HUD transitions
		GameState prev_state = GameState::MAIN_MENU;

		// Input callback - handles both flight and FPS controls based on game state
		visualizer->AddInputCallback([&](const Boidsish::InputState& state) {
			// Reset frame-specific inputs
			controller->ResetFrameInputs();

			// Capture mouse delta for FPS mode
			controller->mouse_delta_x = static_cast<float>(state.mouse_delta_x);
			controller->mouse_delta_y = static_cast<float>(state.mouse_delta_y);

			// Check for any key press (for menu navigation)
			for (int i = 0; i < Constants::Library::Input::MaxKeys(); ++i) {
				if (state.key_down[i]) {
					controller->any_key_pressed = true;
					break;
				}
			}

			GameState current_state = g_game_state.GetState();

			switch (current_state) {
			case GameState::MAIN_MENU:
				// Update camera pan
				UpdateMainMenuCamera(*visualizer, state.delta_time);
				break;
			case GameState::GAME_OVER:
				// Only check for any key to proceed
				break;

			case GameState::FLIGHT_MODE:
			case GameState::LANDING_TRANSITION:
			case GameState::TAKEOFF_TRANSITION:
				// Flight controls
				controller->pitch_up = state.keys[GLFW_KEY_S];
				controller->pitch_down = state.keys[GLFW_KEY_W];
				controller->yaw_left = state.keys[GLFW_KEY_A];
				controller->yaw_right = state.keys[GLFW_KEY_D];
				controller->roll_left = state.keys[GLFW_KEY_Q];
				controller->roll_right = state.keys[GLFW_KEY_E];
				controller->boost = state.keys[GLFW_KEY_LEFT_SHIFT];
				controller->brake = state.keys[GLFW_KEY_LEFT_CONTROL];
				controller->fire = state.keys[GLFW_KEY_SPACE];
				controller->chaff = state.keys[GLFW_KEY_G];
				controller->super_speed = state.keys[GLFW_KEY_B];

				// Landing trigger
				controller->holding_land_key = state.keys[GLFW_KEY_LEFT_CONTROL];

				// Weapon switching
				if (state.key_down[GLFW_KEY_F]) {
					kittywumpus_selected_weapon = (kittywumpus_selected_weapon + 1) % 4;
					g_weapon_selector->SetSelectedIndex(kittywumpus_selected_weapon);
				}
				break;

			case GameState::FIRST_PERSON_MODE:
				// FPS controls
				controller->move_forward = state.keys[GLFW_KEY_W];
				controller->move_backward = state.keys[GLFW_KEY_S];
				controller->move_left = state.keys[GLFW_KEY_A];
				controller->move_right = state.keys[GLFW_KEY_D];
				controller->sprint = state.keys[GLFW_KEY_LEFT_SHIFT];

				// FPS mouse buttons for weapon effects
				controller->mouse_left = state.mouse_buttons[GLFW_MOUSE_BUTTON_LEFT];
				controller->mouse_right = state.mouse_buttons[GLFW_MOUSE_BUTTON_RIGHT];
				controller->mouse_left_released = state.mouse_button_up[GLFW_MOUSE_BUTTON_LEFT];
				controller->mouse_right_released = state.mouse_button_up[GLFW_MOUSE_BUTTON_RIGHT];

				// Takeoff trigger (hold SPACE)
				controller->holding_takeoff_key = state.keys[GLFW_KEY_SPACE];

				// Update takeoff charge display
				if (g_takeoff_charge_indicator) {
					float charge = g_game_state.GetTakeoffChargeProgress() * 100.0f;
					g_takeoff_charge_indicator->SetValue(charge);
				}
				break;
			}

			// Update game state
			g_game_state.Update(state.delta_time, *visualizer, g_plane, *controller);

			// Handle HUD transitions
			GameState new_state = g_game_state.GetState();
			if (new_state != prev_state) {
				switch (new_state) {
				case GameState::MAIN_MENU:
					SetupMainMenuHUD(*visualizer);
					g_handler->SetInMainMenu(true);
					// Clear any game over messages when returning to menu
					if (prev_state == GameState::GAME_OVER) {
						g_handler->ClearGameOverHUD();
					}
					break;
				case GameState::FLIGHT_MODE:
					// Reset plane state when starting new game from menu
					if (prev_state == GameState::MAIN_MENU || prev_state == GameState::GAME_OVER) {
						g_plane->ResetState();
						g_handler->PreparePlane(g_plane);
						if (g_health_gauge) {
							g_health_gauge->SetValue(1.0f);
						}
						if (g_score_indicator) {
							g_score_indicator->SetValue(0);
						}
						// Clear game over HUD if coming from game over
						if (prev_state == GameState::GAME_OVER) {
							g_handler->ClearGameOverHUD();
						}
					}
					SetupFlightHUD(*visualizer);
					g_handler->SetGameStateFlying(true);
					g_handler->SetInMainMenu(false);
					visualizer->SetChaseCamera(g_plane);
					break;
				case GameState::FIRST_PERSON_MODE:
					SetupFPSHUD(*visualizer);
					g_handler->SetGameStateFlying(false);
					break;
				case GameState::GAME_OVER:
					// Game over HUD is set up by handler's OnPlaneDeath
					g_handler->SetInMainMenu(false);
					break;
				default:
					break;
				}
				prev_state = new_state;
			}
		});

		// Add steering probe for checkpoint generation
		g_handler->AddEntity<SteeringProbeEntity>(visualizer->GetTerrain(), g_plane);

		// Start with main menu
		SetupMainMenuHUD(*visualizer);
		g_game_state.SetupMainMenu(*visualizer);

		// Play music
		visualizer->GetAudioManager().PlayMusic("assets/kazoom.mp3", true, 0.25f);

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
