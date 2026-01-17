#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 VertexColor;

uniform vec3 viewPos;

void main()
{
    float distance = length(viewPos - FragPos);
    float alpha = smoothstep(20.0, 60.0, distance);
    FragColor = vec4(VertexColor, alpha);
}
