#ifndef ARTISTIC_EFFECTS_FRAG
#define ARTISTIC_EFFECTS_FRAG

layout(std140) uniform ArtisticEffects {
	bool blackAndWhite;
	bool negative;
	bool shimmery;
	bool glitched;
	bool wireframe;
};

vec3 applyBlackAndWhite(vec3 color) {
	if (blackAndWhite) {
		float average = (color.r + color.g + color.b) / 3.0;
		return vec3(average, average, average);
	}
	return color;
}

vec3 applyNegative(vec3 color) {
	if (negative) {
		return vec3(1.0 - color.r, 1.0 - color.g, 1.0 - color.b);
	}
	return color;
}

vec3 applyShimmery(vec3 color, float time) {
	if (shimmery) {
		float shimmer = (sin(time * 10.0) + 1.0) / 2.0;
		return color + vec3(shimmer * 0.2, shimmer * 0.2, shimmer * 0.2);
	}
	return color;
}

vec3 applyGlitched(vec3 color, float time) {
	if (glitched) {
		float glitch = (sin(time * 50.0) + 1.0) / 2.0;
		if (glitch > 0.9) {
			return color.bgr;
		}
	}
	return color;
}

vec3 applyWireframe(vec3 color, vec3 barycentric) {
	if (wireframe) {
		float edge_factor = min(barycentric.x, min(barycentric.y, barycentric.z));
		if (edge_factor < 0.01) {
			return vec3(0.0, 0.0, 0.0);
		}
	}
	return color;
}

vec3 applyArtisticEffects(vec3 color, vec3 barycentric, float time) {
	color = applyBlackAndWhite(color);
	color = applyNegative(color);
	color = applyShimmery(color, time);
	color = applyGlitched(color, time);
	color = applyWireframe(color, barycentric);
	return color;
}

#endif
