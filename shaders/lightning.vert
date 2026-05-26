#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in float aIntensity;

out vec3 vColor;
out float vIntensity;

uniform mat4 projection;
uniform mat4 view;

void main() {
    vColor = aColor;
    vIntensity = aIntensity;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
