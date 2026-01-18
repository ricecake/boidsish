#version 420 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

out vec3 aPos_vs;
out vec3 aNormal_vs;
out vec2 aTexCoords_vs;

void main()
{
    aPos_vs = aPos;
    aNormal_vs = aNormal;
    aTexCoords_vs = aTexCoords;
}
