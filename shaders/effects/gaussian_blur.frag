#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform bool horizontal;

// 10 samples for a separable gaussian blur
float weights[10] = float[] (0.0006, 0.0023, 0.0088, 0.026, 0.065, 0.121, 0.174, 0.19, 0.153, 0.058);

void main()
{
    vec2 tex_offset = 1.0 / textureSize(image, 0); // gets size of single texel
    vec3 result = texture(image, TexCoords).rgb * weights[0]; // current fragment's contribution

    if(horizontal)
    {
        for(int i = 1; i < 10; ++i)
        {
            result += texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
            result += texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
        }
    }
    else
    {
        for(int i = 1; i < 10; ++i)
        {
            result += texture(image, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weights[i];
            result += texture(image, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weights[i];
        }
    }

    FragColor = vec4(result, 1.0);
}
