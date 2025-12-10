#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 WorldPos;
out vec3 Normal;
out vec4 ClipSpacePos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal = vec3(0.0, 1.0, 0.0);
    ClipSpacePos = projection * view * vec4(WorldPos, 1.0);
    gl_Position = ClipSpacePos;
}
