#version 430 core

#include "lighting.glsl"
#include "helpers/lighting.glsl"

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
    float _pad0;
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
    float _pad0;
    float _pad1;
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

uniform mat4 view;

layout(std140, binding = [[VOLUMETRIC_LIGHTING_BINDING]]) uniform VolumetricLighting {
    mat4 invViewProj;
    mat4 prevViewProj;
    vec4 gridParams;     // x: near, y: far, z: log bias, w: cascade count
    vec4 resolution;     // x: gridW, y: gridH, z: gridD, w: intensity
    vec4 sunDir;         // xyz: dir, w: sun intensity
    vec4 sunColor;       // rgb: color, w: mie anisotropy (g)
    vec4 hazeParams;     // x: haze density, y: haze height, z: noise scale, w: noise strength
    vec4 ambientColor;   // rgb: color, w: scattering scale
    vec4 cloudParams;    // x: cloud coverage, y: cloud density, z: cloud shadow intensity, w: reserved
    vec4 cascadeSplits;
} u_vol;

uniform sampler3D u_volumetricIntegrated[4];

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

    vec3 albedo = mix(biomeProps[fBiomeIdx].colorBottom.rgb, biomeProps[fBiomeIdx].colorTop.rgb, fHeightFactor);
    float ao = smoothstep(0.0, 0.5, fHeightFactor) * smoothstep(0.0, 0.25, biomeProps[fBiomeIdx].density);

    // Add some random variability
    uint seed = uint(abs(fWorldPos.x) * 10.0) ^ uint(abs(fWorldPos.z) * 10.0);
    float var = hash(seed) * biomeProps[fBiomeIdx].colorVariability;
    albedo += (var * 2.0 - 1.0) * 0.15;
    albedo = max(vec3(0.0), albedo); // Clamp to prevent negative color artifacts

    // Normal handling for 2D primitives
    vec3 N = normalize(fNormal);
    if (!gl_FrontFacing) N = -N;

    float primaryShadow;
    vec4 litColor = apply_lighting_pbr(fWorldPos, N, albedo, 0.8, 0.0, ao, primaryShadow);
    litColor.rgb = clamp(litColor.rgb, 0.0, 5.0); // Clamp HDR to prevent "bright white" blowouts

    // Volumetric Lighting Integration
    float viewDist = length(fWorldPos - viewPos);

    int volCascade = 3;
    for (int i = 0; i < 4; ++i) {
        if (viewDist < u_vol.cascadeSplits[i]) {
            volCascade = i;
            break;
        }
    }

    float v_near = (volCascade == 0) ? u_vol.gridParams.x : u_vol.cascadeSplits[volCascade-1];
    float v_far = u_vol.cascadeSplits[volCascade];

    float volZ = log(max(viewDist, v_near) / v_near) / log(v_far / v_near);
    vec4 screenPos = projection * view * vec4(fWorldPos, 1.0);
    vec2 volUV = (screenPos.xy / screenPos.w) * 0.5 + 0.5;
    vec4 volumetric = texture(u_volumetricIntegrated[volCascade], vec3(volUV, volZ));
    litColor.rgb = litColor.rgb * volumetric.a + volumetric.rgb;

    // Distance fade and distant cyan blend (matching terrain style)
    float dist = length(fWorldPos.xz - viewPos.xz);
    float fade_start = 560.0 * worldScale;
    float fade_end = 570.0 * worldScale;
    float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

    if (fade < 0.1) discard;

    vec4 baseColor = vec4(litColor.rgb, fade);
    // Standard engine distant cyan blend logic
    // step(1.0, fade) means: if fade < 1.0 (distant), use cyan.
    // We stay natural until deep into the fade range to avoid "blue glow"
    float cyanFactor = smoothstep(0.0, 0.1, fade);
    FragColor = mix(vec4(0.0, 0.5, 0.5, baseColor.a) * min(length(baseColor.rgb), 1.0) * 0.1, baseColor, cyanFactor);

    // Output view-space normal
    NormalOut = vec4(normalize(mat3(view) * N), primaryShadow);
    AlbedoOut = vec4(albedo, 1.0);
    VelocityOut = vec4(0.0, 0.0, 0.8, 0.0); // No motion, roughness=0.8, metallic=0.0
}
