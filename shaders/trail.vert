#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in float aProgress;

out vec3 vs_color;
out float vs_progress;

void main()
{
    vs_color = aColor;
    vs_progress = aProgress;
    gl_Position = vec4(aPos, 1.0);
}
