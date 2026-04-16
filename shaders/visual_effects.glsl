#ifndef VISUAL_EFFECTS_GLSL
#define VISUAL_EFFECTS_GLSL

layout(std140, binding = [[VISUAL_EFFECTS_BINDING]]) uniform VisualEffects {
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
};

#endif
