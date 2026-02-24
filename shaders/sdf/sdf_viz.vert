#version 460 core
#extension GL_ARB_shader_draw_parameters : enable

layout(location = 0) in vec3 aPos;

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3     FragPos;
out mat4     vModel;
flat out int vUniformIndex;

void main()
{
#ifdef GL_ARB_shader_draw_parameters
	int drawID = gl_DrawIDARB;
#else
	int drawID = gl_DrawID;
#endif

	vUniformIndex = uUseMDI ? drawID : -1;
	mat4 current_model = (uUseMDI && vUniformIndex >= 0) ? uniforms_data[vUniformIndex].model : model;

    FragPos = vec3(current_model * vec4(aPos, 1.0));
    vModel = current_model;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
