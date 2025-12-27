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
		int shimmery_enabled;
		int wireframe_enabled;
	};
} // namespace Boidsish
