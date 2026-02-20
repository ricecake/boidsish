#version 420 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

#include "temporal_data.glsl"

out vec2 TexCoords;
out vec4 CurPosition;
out vec4 PrevPosition;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
	TexCoords = aTexCoords;
	vec4 world_pos = model * vec4(aPos, 1.0);
	gl_Position = projection * view * world_pos;
	CurPosition = gl_Position;
	PrevPosition = prevViewProjection * world_pos;
}
