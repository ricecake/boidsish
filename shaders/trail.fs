#version 330 core
out vec4 FragColor;

in float Progress;

uniform vec3 color;

void main()
{
    FragColor = vec4(color, Progress);
}
