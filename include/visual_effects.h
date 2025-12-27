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
		TWIST,
	};

	struct VisualEffectsUbo {
		int ripple_enabled;
		int color_shift_enabled;
		int black_and_white_enabled;
		int negative_enabled;
		int shimmery_enabled;
		int glitched_enabled;
		int wireframe_enabled;
	int twist_enabled;
	};
} // namespace Boidsish
