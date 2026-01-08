#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

#include "visual_effects.glsl"
#include "visual_effects.vert"

out vec3 WorldPos_VS_out;
out vec2 TexCoords_VS_out;
out vec3 Normal_VS_out;
out vec3 viewForward_in;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 viewForward;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

void main() {
	WorldPos_VS_out = vec3(model * vec4(aPos, 1.0));
	TexCoords_VS_out = aTexCoords;
	Normal_VS_out = mat3(transpose(inverse(model))) * aNormal;

	// Forward vector for focus culling in TCS
	viewForward_in = viewForward;

	// gl_Position must be in clip space for the tessellator to work correctly.
	gl_Position = projection * view * vec4(WorldPos_VS_out, 1.0);
}
