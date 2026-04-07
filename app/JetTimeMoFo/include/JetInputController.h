#pragma once

namespace Boidsish {

	struct JetInputController {
		bool pitch_up = false;
		bool pitch_down = false;
		bool yaw_left = false;
		bool yaw_right = false;
		bool roll_left = false;
		bool roll_right = false;
		bool throttle_up = false;
		bool throttle_down = false;
		bool toggle_stabilization = false;
	};

} // namespace Boidsish
