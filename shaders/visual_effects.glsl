#ifndef VISUAL_EFFECTS_GLSL
#define VISUAL_EFFECTS_GLSL

layout(std140) uniform VisualEffects {
	int ripple_enabled;
	int color_shift_enabled;
	int black_and_white_enabled;
	int negative_enabled;
	int shimmery_enabled;
	int glitched_enabled;
	int wireframe_enabled;
	int distant_curl_enabled;

	float distant_curl_fade_start;
	float distant_curl_fade_end;
	float distant_curl_strength;

	// Pad to a multiple of 16 bytes.
	float pad1;
};

#endif
