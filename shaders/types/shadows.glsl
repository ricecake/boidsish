#ifndef SHADOW_TYPES_GLSL
#define SHADOW_TYPES_GLSL

const int MAX_SHADOW_MAPS = [[MAX_SHADOW_MAPS]];

// Shadow mapping UBO (binding set via glUniformBlockBinding to point 2)
layout(std140, binding = [[SHADOWS_BINDING]]) uniform Shadows {
	mat4 lightSpaceMatrices[MAX_SHADOW_MAPS];
	vec4 cascadeSplits;
	int  numShadowLights;
};

// Per-light shadow map index (-1 if no shadow for this light)
// This is set via uniform since the Light struct can't easily store it
uniform int lightShadowIndices[[[MAX_LIGHTS]]];

#endif
