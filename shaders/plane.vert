#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 WorldPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal = vec3(0.0, 1.0, 0.0); // The plane is at y=0, so the normal is straight up.
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
