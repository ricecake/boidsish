#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uSsrTexture;
uniform sampler2D uAlbedoMetallicTexture;
uniform float uIntensity = 1.0;

void main() {
    vec3 sceneColor = texture(uSceneTexture, TexCoords).rgb;
    vec4 ssrData = texture(uSsrTexture, TexCoords);
    vec4 albedoMetallic = texture(uAlbedoMetallicTexture, TexCoords);

    vec3 reflectionColor = ssrData.rgb;
    float reflectionMask = ssrData.a;
    float metallic = albedoMetallic.a;

    // Simple Fresnel approximation
    float fresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - TexCoords.y, 5.0); // Rough approximation

    vec3 finalColor = sceneColor + reflectionColor * reflectionMask * uIntensity * mix(fresnel, 1.0, metallic);

    FragColor = vec4(finalColor, 1.0);
}
