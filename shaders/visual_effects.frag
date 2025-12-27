#ifndef ARTISTIC_EFFECTS_FRAG
#define ARTISTIC_EFFECTS_FRAG

#include "visual_effects.glsl"

vec3 applyShimmery(vec3 color, float time) {
	if (shimmery_enabled == 1) {
		float shimmer = (sin(time * 10.0) + 1.0) / 2.0;
		return color + vec3(shimmer * 0.2, shimmer * 0.2, shimmer * 0.2);
	}
	return color;
}

vec3 applyWireframe(vec3 color, vec3 barycentric) {
	if (wireframe_enabled == 1) {
		float edge_factor = min(barycentric.x, min(barycentric.y, barycentric.z));
		if (edge_factor < 0.01) {
			return vec3(0.0, 0.0, 0.0);
		}
	}
	return color;
}

vec3 applyArtisticEffects(vec3 color, vec3 fragPos, vec3 barycentric, float time) {
	color = applyShimmery(color, time);
	color = applyWireframe(color, barycentric);
	return color;
}

#endif
