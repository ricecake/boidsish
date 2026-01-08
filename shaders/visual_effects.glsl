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
};

#endif

vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 applyGlitch(vec3 position, float time) {
	if (glitched_enabled == 1) {
		float glitchStrength = 0.1;
		float glitchSpeed = 10.0;
		vec3 displacement = sin(position * glitchSpeed + time * 10.0) * glitchStrength;
		position += mix(
			smoothstep(displacement.yzx / 2, 3 * displacement.zxy / 2, abs(sin(time * mod289(position)))),
			mix(displacement.yzx / 2, 3 * displacement.zxy / 2, abs(sin(time * mod289(position)))),
			abs(sin(time))
		);
	}
	return position;
}
