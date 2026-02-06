#pragma once

namespace Boidsish {

	struct PaperPlaneInputController {
		bool pitch_up = false;
		bool pitch_down = false;
		bool yaw_left = false;
		bool yaw_right = false;
		bool roll_left = false;
		bool roll_right = false;
		bool boost = false;
		bool brake = false;
		bool fire = false;
		bool chaff = false;
		bool super_speed = false;
	};

} // namespace Boidsish
