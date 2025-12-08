#version 330 core
layout (location = 0) in vec3 aPos;

out vec2 screenCoord;

void main()
{
    screenCoord = aPos.xy;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
