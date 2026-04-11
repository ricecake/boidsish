#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D volumetricTexture;

void main() {
    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
    vec4 volumetric = texture(volumetricTexture, TexCoords);

    // Volumetric.rgb is the in-scattered light
    // Volumetric.a is the transmittance (not used here for additive blend, but useful for fog)

    FragColor = vec4(sceneColor + volumetric.rgb, 1.0);
}
