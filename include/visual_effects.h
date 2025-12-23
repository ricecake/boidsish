#pragma once

namespace Boidsish {
	enum class VisualEffect {
		RIPPLE,
		COLOR_SHIFT,
	BLACK_AND_WHITE,
	NEGATIVE,
	SHIMMERY,
	GLITCHED,
	WIREFRAME,
	};

	struct VisualEffectsUbo {
		int ripple_enabled;
		int color_shift_enabled;
		// Add other effect states here
	};
} // namespace Boidsish
