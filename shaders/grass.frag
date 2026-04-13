#version 430 core

#include "helpers/lighting.glsl"

in vec3 fNormal;
in vec2 fTexCoords;
in float fHeightFactor;
in vec3 fWorldPos;

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
    uint  biomeMask;
    float windInfluence;
    float padding[2];
};

layout(std140, binding = 9) uniform GrassProps {
    GrassProperties props;
};

uniform bool uIsShadowPass;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalOut;
layout(location = 2) out vec4 VelocityOut;
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
        return;
    }

    vec3 albedo = mix(props.colorBottom.rgb, props.colorTop.rgb, fHeightFactor);

    // Add some random variability
    uint seed = uint(abs(fWorldPos.x) * 10.0) ^ uint(abs(fWorldPos.z) * 10.0);
    float var = hash(seed) * props.colorVariability;
    albedo += (var * 2.0 - 1.0) * 0.15;

    float primaryShadow;
    vec4 litColor = apply_lighting_pbr(fWorldPos, normalize(fNormal), albedo, 0.8, 0.0, 1.0, primaryShadow);

    FragColor = vec4(litColor.rgb, 1.0);
    NormalOut = vec4(normalize(fNormal) * 0.5 + 0.5, 1.0);
    AlbedoOut = vec4(albedo, 1.0);
    VelocityOut = vec4(0.0, 0.0, 0.0, 1.0);
}
