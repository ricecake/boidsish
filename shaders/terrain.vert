#version 460 core

layout(location = 0) in vec3 aPos;             // Prebaked static world position
layout(location = 1) in vec3 aNormal;          // Prebaked static normal
layout(location = 2) in vec2 aTexCoords;       // UV coordinate [0, 1]
layout(location = 3) in float aTextureSlice;   // Texture slice index
layout(location = 4) in float aPerturbFactor;  // Perturb factor
layout(location = 5) in float aTessFactor;     // Tessellation factor (LOD indicator)
layout(location = 6) in float aIsWater;        // Water mask / alpha
layout(location = 7) in float aErosionDelta;   // Erosion delta
layout(location = 8) in float aRidgeMap;       // Ridge map
layout(location = 9) in float aSubstrate;      // Substrate value

#include "helpers/constants.glsl"
#include "helpers/lighting.glsl"
#include "helpers/shockwave.glsl"
#include "temporal_data.glsl"
#include "visual_effects.glsl"

out vec3       Normal;
out vec3       FragPos;
out vec4       CurPosition;
out vec4       PrevPosition;
out vec2       TexCoords;
flat out float TextureSlice;
out float      perturbFactor;
out float      tessFactor;
out float      vIsWater;
out float      vErosionDelta;
out float      vRidgeMap;
out float      vSubstrate;

void main() {
    FragPos = aPos;
    Normal = normalize(aNormal);
    TexCoords = aTexCoords;
    TextureSlice = aTextureSlice;
    perturbFactor = aPerturbFactor;
    tessFactor = aTessFactor;
    vIsWater = aIsWater;
    vErosionDelta = aErosionDelta;
    vRidgeMap = aRidgeMap;
    vSubstrate = aSubstrate;

    float waterMask = aIsWater;

    // Apply dynamic water ripple animation
    if (waterMask > 0.0) {
        float landHeight = FragPos.y;
        float waterHeight = 0.0;

        // Add gentle ripple displacement
        float rippleTime = time * 2.0;
        float ripple = sin(FragPos.x * 0.5 + rippleTime) * 0.05 + cos(FragPos.z * 0.5 + rippleTime * 0.8) * 0.05;
        waterHeight += ripple;

        // Approximate normal for the ripple surface
        float dx = 0.05 * cos(FragPos.x * 0.5 + rippleTime) * 0.5;
        float dz = 0.05 * -sin(FragPos.z * 0.5 + rippleTime * 0.8) * 0.4;
        vec3 waterNormal = normalize(vec3(-dx, 1.0, -dz));

        FragPos.y = mix(landHeight, waterHeight, waterMask);
        Normal = normalize(mix(Normal, waterNormal, waterMask));
    }

    // Apply shockwave ripple displacement to terrain
    FragPos += getShockwaveDisplacement(FragPos, 0.0, false);

    // Dynamic Projection
    gl_Position = projection * view * vec4(FragPos, 1.0);
    CurPosition = gl_Position;
    PrevPosition = prevViewProjection * vec4(FragPos, 1.0);
}
