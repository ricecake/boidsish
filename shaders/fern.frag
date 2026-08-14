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

    // Leaf details created by shading portions of the strip transparently
    float u = fTexCoords.x * 2.0 - 1.0; // -1 to 1 across frond
    float v = fTexCoords.y;             // 0 to 1 along frond

    float distFromCenter = abs(u);
    // Leaflet pattern using a fast sine/cosine function along the frond
    float leafletLine = sin(v * 75.0 - distFromCenter * 6.0);

    float leafMask = 1.0;
    if (distFromCenter > 0.12) {
        // Create gaps/leaflets
        leafMask = smoothstep(-0.2, 0.2, leafletLine);
        // Taper out at the edge of the strip
        leafMask *= smoothstep(1.0, 0.8, distFromCenter);
    }

    // If leafMask is low, we shade it transparently (discard)
    if (leafMask < 0.25) {
        discard;
    }

    // Normal handling for 2D primitives
    vec3 N = normalize(fNormal);
    if (!gl_FrontFacing) N = -N;

    // Ferns have lush forest-like green colors
    vec3 colorBase = vec3(0.08, 0.28, 0.08); // Darker bottom/middle
    vec3 colorTip = vec3(0.18, 0.52, 0.15);  // Brighter tips
    vec3 albedo = mix(colorBase, colorTip, v);

    float roughness = 0.75;

    // Apply wetness: darkening and reduction in roughness
    albedo = mix(albedo, albedo * 0.4, wetness * 0.6);
    roughness = mix(roughness, 0.1, wetness * 0.9);

    float ao = smoothstep(0.1, 0.6, v) * smoothstep(0.0, 0.25, biomeProps[fBiomeIdx].density);
    float dist = length(fWorldPos.xz - viewPos.xz);

    // Add some random variability
    uint seed = uint(abs(fWorldPos.x) * 11.0) ^ uint(abs(fWorldPos.z) * 11.0);
    float var = hash(seed) * biomeProps[fBiomeIdx].colorVariability;
    albedo += (var * 2.0 - 1.0) * 0.1;
    albedo = max(vec3(0.0), albedo);

    // Apply wind-driven rim highlight
    float rim = pow(1.0 - max(dot(N, normalize(viewPos - fWorldPos)), 0.0), 3.0);
    albedo += rim * smoothstep(0.33, 0.66, v) * smoothstep(1.0, 0.66, v) * vec3(0.6, 0.9, 0.6);

    vec3 highlight = albedo * mix(
        smoothstep(0.05, 0.25, length(u)) * smoothstep(0.95, 0.75, length(u)),
        1.0,
        smoothstep(16.0, 128.0, length(viewPos - fWorldPos))
    );

    float primaryShadow;
    vec4 litColor = apply_lighting_foliage(fWorldPos, N, albedo, roughness, 0.0, ao, primaryShadow);
    float scaleFactor = 1.0;
    if (num_lights > 0) {
        scaleFactor = max(1.0, lights[0].intensity / 10.0);
    }
    litColor = min(litColor, vec4(highlight * scaleFactor, litColor.a));
    litColor.rgb = clamp(litColor.rgb, 0.0, 5.0 * scaleFactor);

    // Distance fade
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
