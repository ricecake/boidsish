#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aProgress;

out float vs_progress;

void main()
{
    vs_progress = aProgress;
    gl_Position = vec4(aPos, 1.0);
}
