#version 460 core

layout(location = 0) in vec3 aPos;

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
uniform int  uBaseUniformIndex = 0;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main() {
	int  vUniformIndex = uUseMDI ? (uBaseUniformIndex + gl_DrawID) : -1;
	bool use_ssbo = uUseMDI && vUniformIndex >= 0;
	mat4 current_model = use_ssbo ? uniforms_data[vUniformIndex].model : model;

	gl_Position = lightSpaceMatrix * current_model * vec4(aPos, 1.0);
}
