#pragma once

#include "PaperPlaneInputController.h"

namespace Boidsish {

// INTEGRATION_POINT: Extend with additional controls for future features
// (e.g., weapon_switch_fps, interact, reload, crouch)
struct KittywumpusInputController : PaperPlaneInputController {
	// FPS controls (active in FIRST_PERSON_MODE)
	bool move_forward = false;   // W in FPS mode
	bool move_backward = false;  // S in FPS mode
	bool move_left = false;      // A in FPS mode
	bool move_right = false;     // D in FPS mode
	bool sprint = false;         // SHIFT in FPS mode
	float mouse_delta_x = 0.0f;
	float mouse_delta_y = 0.0f;

	// FPS mouse buttons (for weapon effects)
	bool mouse_left = false;          // Left click held
	bool mouse_right = false;         // Right click held
	bool mouse_left_released = false; // Left click just released
	bool mouse_right_released = false;// Right click just released

	// Transition triggers
	bool holding_land_key = false;    // CTRL - for landing (when low altitude in FLIGHT_MODE)
	bool holding_takeoff_key = false; // SPACE - for takeoff (hold 3 seconds in FIRST_PERSON_MODE)

	// Menu/restart control
	bool any_key_pressed = false;     // Any key - for menu transitions

	// Reset all inputs (useful between frames)
	void ResetFrameInputs() {
		mouse_delta_x = 0.0f;
		mouse_delta_y = 0.0f;
		any_key_pressed = false;
		mouse_left_released = false;
		mouse_right_released = false;
	}
};

} // namespace Boidsish
