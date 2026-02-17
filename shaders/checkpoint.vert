#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
uniform int  uBaseUniformIndex = 0;

out vec2 TexCoords;
flat out int vUniformIndex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
	vUniformIndex = uUseMDI ? (uBaseUniformIndex + gl_DrawID) : -1;
	mat4 current_model = uUseMDI ? uniforms_data[vUniformIndex].model : model;

	TexCoords = aTexCoords;
	gl_Position = projection * view * current_model * vec4(aPos, 1.0);
}
