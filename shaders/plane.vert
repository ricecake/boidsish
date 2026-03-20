#version 430 core
layout(location = 0) in vec3 aPos;

out vec3 WorldPos;
out vec3 Normal;
out vec4 CurPosition;
out vec4 PrevPosition;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

#include "helpers/lighting.glsl"
#include "temporal_data.glsl"
#include "visual_effects.glsl"

void main() {
	WorldPos = vec3(model * vec4(aPos, 1.0));
	WorldPos.xz += viewPos.xz;
	Normal = vec3(0.0, 1.0, 0.0);
	vec4 vPos = view * vec4(WorldPos, 1.0);
	vPos = warpViewSpace(vPos);
	gl_Position = projection * vPos;

	CurPosition = gl_Position;
	vec4 prevVPos = prevView * vec4(WorldPos, 1.0);
	prevVPos = warpViewSpace(prevVPos);
	PrevPosition = uProjection * prevVPos;
}
