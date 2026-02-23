#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uAccumulatedGI;
uniform float uIntensity;

void main() {
    vec3 sceneColor = texture(uSceneTexture, TexCoords).rgb;
    vec3 indirectLight = texture(uAccumulatedGI, TexCoords).rgb;

    // Combine with scene
    vec3 result = sceneColor + indirectLight * uIntensity;

    FragColor = vec4(result, 1.0);
}
