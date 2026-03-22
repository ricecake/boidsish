#version 460 core
layout (location = 0) in vec3 aPos;

#include "../common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;

void main() {
    mat4 current_model = (uUseMDI && gl_DrawID >= 0) ? uniforms_data[gl_DrawID].model : model;
    FragPos = vec3(current_model * vec4(aPos, 1.0));
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
