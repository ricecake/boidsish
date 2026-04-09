#version 430 core
#include "helpers/bindless.glsl"

out vec4 FragColor;

in vec2 TexCoords;

BINDLESS_SAMPLER2D(u_Texture);

void main()
{
    FragColor = texture(u_Texture, TexCoords);
}
