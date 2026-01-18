#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;

void main()
{
    // Sample the HDR texture
    vec3 hdrColor = texture(sceneTexture, TexCoords).rgb;

    // Apply Reinhard tone mapping
    // This maps the color to a range of [0, 1]
    vec3 mappedColor = hdrColor / (hdrColor + vec3(1.0));

    // The output is now in LDR.
    FragColor = vec4(mappedColor, 1.0);
}
