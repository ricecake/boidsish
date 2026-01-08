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

#endif
