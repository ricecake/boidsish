#version 460 core

#define USE_TERRAIN_DATA
#include "helpers/terrain_shadows.glsl"
#include "lighting.glsl"
#include "helpers/lighting.glsl"
#include "visual_effects.glsl"

in vec3 fNormal;
in vec2 fTexCoords;
in float fHeightFactor;
in vec3 fWorldPos;
flat in int fBiomeIdx;
in float fIsLeaf;
in float fCanopyAo;

struct GrassProperties {
    vec4  colorTop;
    vec4  colorBottom;
    float height;
    float width;
    float rigidity;
    float heightVariance;
    float widthVariance;
    float density;
    float colorVariability;
    float windInfluence;
    uint  enabled;
    float flowerRatio;
    float _pad1;
    float _pad2;
};

struct GlobalGrassProperties {
    float lengthMultiplier;
    float widthMultiplier;
    float densityMultiplier;
    float rigidityMultiplier;
    float windMultiplier;
    uint  enabled;
    float lodScaleFactor;
    float lodBaseRange;
    float baseScale;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std140, binding = [[GRASS_PROPS_BINDING]]) uniform GrassProps {
    GrassProperties biomeProps[8];
    GlobalGrassProperties globalProps;
};

uniform bool uIsShadowPass;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 VelocityOut;
layout(location = 2) out vec4 NormalOut;
layout(location = 3) out vec4 AlbedoOut;

// Simple hash for random values
float hash(uint x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return float(x) / 4294967295.0;
}

void main() {
    if (uIsShadowPass) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 N = normalize(fNormal);
    if (!gl_FrontFacing && fIsLeaf > 0.5) N = -N;

    vec3 albedo;
    float roughness;
    float ao = fCanopyAo;

    uint seed = uint(abs(fWorldPos.x) * 13.0) ^ uint(abs(fWorldPos.z) * 17.0);

    if (fIsLeaf < 0.5) {
        // --- TRUNK BARK ---
        // Bark color responds to biome soil/bottom color
        vec3 baseBarkColor = vec3(0.24, 0.17, 0.12); // Natural wood bark
        vec3 biomeSoilColor = biomeProps[fBiomeIdx].colorBottom.rgb;
        baseBarkColor = mix(baseBarkColor, biomeSoilColor * 0.7, 0.3);

        // Bark groove texture along vertical height
        float groove = sin(fWorldPos.y * 14.0 + fWorldPos.x * 3.0) * 0.08 + (hash(seed) - 0.5) * 0.05;
        albedo = baseBarkColor + vec3(groove);
        albedo = max(vec3(0.02), albedo);

        roughness = 0.85;
        ao *= smoothstep(0.0, 0.25, fHeightFactor);
    } else {
        // --- LEAF CANOPY ---
        // Leaf cluster circular alpha mask
        vec2 leafUV = (fTexCoords - vec2(0.5)) * 2.0;
        float d = length(leafUV);

        // Discard outer corners for rounded cluster quads
        if (d > 0.95) discard;

        // Leaf color responds to biome grass top & bottom color settings
        vec3 leafBottom = biomeProps[fBiomeIdx].colorBottom.rgb * 0.8;
        vec3 leafTop = biomeProps[fBiomeIdx].colorTop.rgb * 1.15;

        // Vertical and inner/outer color gradient
        float colorFactor = clamp(fHeightFactor * 0.8 + (1.0 - d) * 0.3, 0.0, 1.0);
        albedo = mix(leafBottom, leafTop, colorFactor);

        // Procedural color variation per instance
        float var = (hash(seed) * 2.0 - 1.0) * biomeProps[fBiomeIdx].colorVariability;
        albedo += var * 0.12;
        albedo = max(vec3(0.01), albedo);

        roughness = 0.70;

        // Leaf rim lighting highlight
        float rim = pow(1.0 - max(dot(N, normalize(viewPos - fWorldPos)), 0.0), 3.0);
        albedo += rim * smoothstep(0.3, 0.8, colorFactor) * vec3(0.2, 0.35, 0.15);
    }

    // Apply wetness
    albedo = mix(albedo, albedo * 0.4, wetness * 0.6);
    roughness = mix(roughness, 0.1, wetness * 0.9);

    float dist = length(fWorldPos.xz - viewPos.xz);

    float primaryShadow;
    vec4 litColor = apply_lighting_foliage(fWorldPos, N, albedo, roughness, 0.0, ao, primaryShadow);

    float scaleFactor = 1.0;
    if (num_lights > 0) {
        scaleFactor = max(1.0, lights[0].intensity / 10.0);
    }
    litColor.rgb = clamp(litColor.rgb, 0.0, 5.0 * scaleFactor);

    // Distance fade matching terrain and grass style
    float fade_start = 560.0 * worldScale;
    float fade_end = 570.0 * worldScale;
    float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

    if (fade < 0.1) discard;

    vec4 baseColor = vec4(litColor.rgb, fade);
    float cyanFactor = smoothstep(0.0, 0.1, fade);
    FragColor = mix(vec4(0.0, 0.5, 0.5, baseColor.a) * min(length(baseColor.rgb), 1.0) * 0.1, baseColor, cyanFactor);

    // Output view-space normal
    NormalOut = vec4(normalize(mat3(view) * N), primaryShadow);
    AlbedoOut = vec4(albedo, 1.0);
    VelocityOut = vec4(0.0, 0.0, roughness, 0.0);
}
