#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uSsrTexture;
uniform sampler2D uAlbedoMetallicTexture;
uniform float uIntensity = 1.0;

#include "temporal_data.glsl"

uniform sampler2D uDepthTexture;
uniform sampler2D uNormalRoughnessTexture;

void main() {
    vec3 sceneColor = texture(uSceneTexture, TexCoords).rgb;
    vec4 ssrData = texture(uSsrTexture, TexCoords);
    vec4 albedoMetallic = texture(uAlbedoMetallicTexture, TexCoords);

    vec3 reflectionColor = ssrData.rgb;
    float reflectionMask = ssrData.a;
    float metallic = albedoMetallic.a;

    // PBR Fresnel
    float depth = texture(uDepthTexture, TexCoords).r;
    vec4 normRough = texture(uNormalRoughnessTexture, TexCoords);
    vec3 viewNormal = normalize(normRough.xyz * 2.0 - 1.0);

    vec3 ndcPos = vec3(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0);
    vec4 viewPos4 = invProjection * vec4(ndcPos, 1.0);
    vec3 viewDir = normalize(-viewPos4.xyz / viewPos4.w);

    float cosTheta = max(dot(viewNormal, viewDir), 0.0);
    float F0 = mix(0.04, 1.0, metallic); // Simplified F0
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);

    // For non-metals, we blend based on fresnel (energy conservation)
    // For metals, the reflection IS the color
    vec3 finalColor;
    if (metallic > 0.5) {
        finalColor = mix(sceneColor, reflectionColor, fresnel * reflectionMask * uIntensity);
    } else {
        // Non-metal: mix reflection on top of scene color based on fresnel
        finalColor = sceneColor + reflectionColor * reflectionMask * uIntensity * fresnel;
    }

    FragColor = vec4(finalColor, 1.0);
}
