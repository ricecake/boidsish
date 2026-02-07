#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

struct Light {
	vec3  position;
	float intensity;
	vec3  color;
	int   type;
	vec3  direction;
	float inner_cutoff; // Also: emissive_radius (EMISSIVE), flash_radius (FLASH)
	float outer_cutoff; // Also: falloff_exp (FLASH)
};

const int MAX_LIGHTS = [[MAX_LIGHTS]];
const int MAX_SHADOW_MAPS = [[MAX_SHADOW_MAPS]];
const int MAX_CASCADES = [[MAX_CASCADES]];

layout(std140, binding = 0) uniform Lighting {
	Light lights[MAX_LIGHTS];
	int   num_lights;
	vec3  viewPos;
	vec3  ambient_light;
	float time;
	vec3  viewDir;
};

// Shadow mapping UBO (binding set via glUniformBlockBinding to point 2)
layout(std140, binding = 2) uniform Shadows {
	mat4 lightSpaceMatrices[MAX_SHADOW_MAPS];
	vec4 cascadeSplits;
	int  numShadowLights;
};

// Shadow map texture array - bound to texture unit 4
layout(binding = 4) uniform sampler2DArrayShadow shadowMaps;

// Per-light shadow map index (-1 if no shadow for this light)
// This is set via uniform since the Light struct can't easily store it
uniform int lightShadowIndices[MAX_LIGHTS];

#endif
