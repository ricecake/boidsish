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

	struct alignas(16) VisualEffectsUbo {
		int   ripple_enabled;
		int   color_shift_enabled;
		int   black_and_white_enabled;
		int   negative_enabled;
		int   shimmery_enabled;
		int   glitched_enabled;
		int   wireframe_enabled;
		int   erosion_enabled;
		float wind_strength;
		float wind_speed;
		float wind_frequency;
		float erosion_strength;
		float erosion_scale;
		float erosion_detail;
		float erosion_gully_weight;
		float erosion_max_dist;
		float rain_intensity;
		float snow_intensity;
		float wetness;
		float _pad_vfx;
		int   particles_enabled;
		float weight_leaf;
		float weight_petal;
		float weight_bubble;
		float weight_snow;
		float weight_firefly;
		float weight_bird;
		float _pad_particles[1];
	};
} // namespace Boidsish
