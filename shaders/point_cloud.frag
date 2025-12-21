#version 420 core
out vec4 FragColor;

in float Value;

uniform float threshold;

void main()
{
    if (Value < threshold)
    {
        discard;
    }
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
