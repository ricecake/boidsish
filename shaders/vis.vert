#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

out vec3 vs_color_in;

void main() {
    gl_Position = vec4(aPos, 1.0);
    vs_color_in = aColor;
}