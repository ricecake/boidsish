#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

struct Light {
	vec3  position;
	float intensity;
	vec3  color;
	float padding; // for std140 alignment
};

const int MAX_LIGHTS = 10;
const int MAX_SHADOW_LIGHTS = 4;

layout(std140) uniform Lighting {
	Light lights[MAX_LIGHTS];
	int   num_lights;
	vec3  viewPos;
	float time;
};

// Shadow mapping UBO (binding set via glUniformBlockBinding to point 2)
layout(std140) uniform Shadows {
	mat4 lightSpaceMatrices[MAX_SHADOW_LIGHTS];
	int  numShadowLights;
};

// Shadow map texture array - bound to texture unit 4
uniform sampler2DArrayShadow shadowMaps;

// Per-light shadow map index (-1 if no shadow for this light)
// This is set via uniform since the Light struct can't easily store it
uniform int lightShadowIndices[MAX_LIGHTS];

#endif
