#version 460 core
#extension GL_ARB_shader_draw_parameters : enable

layout(location = 0) in vec3 aPos;

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main() {
#ifdef GL_ARB_shader_draw_parameters
	int drawID = gl_DrawIDARB;
#else
	int drawID = gl_DrawID;
#endif

	int  vUniformIndex = uUseMDI ? drawID : -1;
	bool use_ssbo = uUseMDI && vUniformIndex >= 0;
	mat4 current_model = use_ssbo ? uniforms_data[vUniformIndex].model : model;

	gl_Position = lightSpaceMatrix * current_model * vec4(aPos, 1.0);
}
