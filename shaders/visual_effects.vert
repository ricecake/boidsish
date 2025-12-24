#ifndef ARTISTIC_EFFECTS_VERT
#define ARTISTIC_EFFECTS_VERT

#include "visual_effects.glsl"

vec3 getBarycentric() {
	if (gl_VertexID % 3 == 0) {
		return vec3(1.0, 0.0, 0.0);
	} else if (gl_VertexID % 3 == 1) {
		return vec3(0.0, 1.0, 0.0);
	} else {
		return vec3(0.0, 0.0, 1.0);
	}
}

vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 applyGlitch(vec3 position, float time) {
	if (glitched_enabled == 1) {
		float glitchStrength = 0.1;
		float glitchSpeed = 10.0;
		// float displacement = sin(position.y * glitchSpeed + time * 10.0) * glitchStrength;
		// position.x += smoothstep(displacement/2, 3*displacement/2, fract(abs(sin(time))));

		// vec3 displacement = sin(position* glitchSpeed + time * 10.0) * glitchStrength;
		// position += smoothstep(displacement.yzx/2, 3*displacement.zxy/2, abs(sin(time*mod289(position))));

		// vec3 displacement = sin(position* glitchSpeed + time * 10.0) * glitchStrength;
		// position += mix(displacement.yzx/2, 3*displacement.zxy/2, abs(sin(time*mod289(position))));

		vec3 displacement = sin(position * glitchSpeed + time * 10.0) * glitchStrength;
		position += mix(
			smoothstep(displacement.yzx / 2, 3 * displacement.zxy / 2, abs(sin(time * mod289(position)))),
			mix(displacement.yzx / 2, 3 * displacement.zxy / 2, abs(sin(time * mod289(position)))),
			abs(sin(time))
		);
	}
	return position;
}

#endif
