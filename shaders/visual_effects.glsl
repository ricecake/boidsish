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
	float wind_strength;
	float wind_speed;
	float wind_frequency;
	float world_scale_exponent;
	float world_scale_reference;
};

// Warps view-space position for non-linear world scaling.
// Radial scaling is invariant under perspective projection, so we scale X and Y
// relative to Z to change perceived size.
vec4 warpViewSpace(vec4 viewPos) {
	if (abs(world_scale_exponent - 1.0) < 0.0001) {
		return viewPos;
	}

	// We use the distance from camera for the scaling factor calculation
	float dist = length(viewPos.xyz);
	if (dist < 0.0001) {
		return viewPos;
	}

	// Scale factor k such that perceived size S ~ 1/dist^exponent
	// k = (dist / reference) ^ (1.0 - exponent)
	float k = pow(dist / world_scale_reference, 1.0 - world_scale_exponent);

	// Apply scaling to X and Y only to avoid perspective cancellation
	vec3 warpedPos = vec3(viewPos.xy * k, viewPos.z);

	return vec4(warpedPos, viewPos.w);
}

#endif
