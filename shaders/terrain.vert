#version 420 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

out vec3 WorldPos_VS_out;
out vec2 TexCoords_VS_out;

uniform mat4 model;

void main() {
    gl_Position = vec4(aPos, 1.0);
    WorldPos_VS_out = vec3(model * vec4(aPos, 1.0));
    TexCoords_VS_out = aTexCoords;
}
