#ifndef ARTISTIC_EFFECTS_VERT
#define ARTISTIC_EFFECTS_VERT

#include "artistic_effects.glsl"

vec3 getBarycentric() {
	if (gl_VertexID % 3 == 0) {
		return vec3(1.0, 0.0, 0.0);
	} else if (gl_VertexID % 3 == 1) {
		return vec3(0.0, 1.0, 0.0);
	} else {
		return vec3(0.0, 0.0, 1.0);
	}
}

vec3 applyGlitch(vec3 position, float time) {
	if (glitched) {
		float glitchStrength = 0.1;
		float glitchSpeed = 10.0;
		float displacement = sin(position.y * glitchSpeed + time * 10.0) * glitchStrength;
		position.x += displacement;
	}
	return position;
}

#endif
