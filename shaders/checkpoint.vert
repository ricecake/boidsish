#version 420 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 vColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool is_instanced;

layout(location = 3) in mat4 instanceMatrix;
layout(location = 7) in vec4 instanceColor;

uniform vec3 baseColor;

void main() {
	TexCoords = aTexCoords;
	vColor = is_instanced ? instanceColor.rgb : baseColor;
	mat4 modelMatrix = is_instanced ? instanceMatrix : model;
	gl_Position = projection * view * modelMatrix * vec4(aPos, 1.0);
}
