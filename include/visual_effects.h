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
		FREEZE_FRAME_TRAIL,
	};

	struct VisualEffectsUbo {
		int   ripple_enabled;
		int   color_shift_enabled;
		int   black_and_white_enabled;
		int   negative_enabled;
		int   shimmery_enabled;
		int   glitched_enabled;
		int   wireframe_enabled;
		int   terrain_shadow_debug;
		float wind_strength;
		float wind_speed;
		float wind_frequency;
		float _pad; // Padding for 16-byte alignment
	};
} // namespace Boidsish
