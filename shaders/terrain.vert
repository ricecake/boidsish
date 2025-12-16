#version 420 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

out vec3 WorldPos_VS_out;
out vec2 TexCoords_VS_out;
out vec3 Normal_VS_out;

uniform mat4 model;

void main() {
    gl_Position = vec4(aPos, 1.0);
    WorldPos_VS_out = vec3(model * vec4(aPos, 1.0));
    TexCoords_VS_out = aTexCoords;
    Normal_VS_out = mat3(transpose(inverse(model))) * aNormal;
}
