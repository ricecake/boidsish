#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

#endif
