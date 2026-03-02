#ifndef VISUAL_EFFECTS_GLSL
#define VISUAL_EFFECTS_GLSL

layout(std140) uniform VisualEffects {
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
};

#endif
