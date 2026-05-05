#version 460 core

#include "../helpers/wind.glsl"
#include "../helpers/fast_noise.glsl"
#include "../types/temporal_data.glsl"

in vec2 TexCoords;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_screenTexture;
layout(binding = 1) uniform sampler2D u_depthTexture;

uniform float u_time;
uniform float u_windLineIntensity;
uniform float u_heatShimmerIntensity;

vec3 worldPosFromDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    vec4 clipPos = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = invProjection * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = invView * viewPos;
    return worldPos.xyz;
}

void main() {
    float depth = texture(u_depthTexture, TexCoords).r;

    // Background handling (infinity)
    if (depth >= 1.0) {
        FragColor = texture(u_screenTexture, TexCoords);
        return;
    }

    vec3 worldPos = worldPosFromDepth(depth);
    vec3 cameraPos = invView[3].xyz;
    vec3 relativePos = worldPos - cameraPos;

    // 1. Heat Shimmer / Refraction
    vec4 scalars = getWeatherScalarsAtPosition(worldPos);
    float temperature = scalars.x; // Kelvin

    vec2 distortedUV = TexCoords;
    float shimmerFactor = smoothstep(310.0, 340.0, temperature) * u_heatShimmerIntensity;

    if (shimmerFactor > 0.0) {
        // Anchor noise to camera to prevent precision-related jitter far from origin
        // Use a higher frequency and relative pos
        float noise = fastSimplex3d(vec3(relativePos.xz * 1.5, u_time * 12.0));
        distortedUV += noise * 0.004 * shimmerFactor;
    }

    vec4 baseColor = texture(u_screenTexture, distortedUV);

    // 2. Stylized Wind Lines
    vec3 wind = getWindAtPosition(worldPos);
    float windSpeed = length(wind);

    if (windSpeed > 1.0 && u_windLineIntensity > 0.0) {
        vec3 windDir = wind / windSpeed;

        // To create stylized "lines", we advect noise along the wind vector
        // and stretch the sampling space dramatically along that same vector.
        vec3 advectedPos = relativePos - wind * u_time * 0.5;

        // Sample coordinates: stretch along windDir by squashing the coordinate component parallel to it
        vec3 lineCoords = advectedPos * 0.8; // Base density
        lineCoords -= windDir * dot(lineCoords, windDir) * 0.98; // 98% stretch along flow

        float noiseValue = fastSimplex3d(lineCoords);

        // Sharp stylized lines using high contrast smoothstep
        float lineMask = smoothstep(0.4, 0.5, noiseValue) * smoothstep(0.8, 0.7, noiseValue);

        // Faint stylized lines
        vec3 lineColor = vec3(0.85, 0.92, 1.0);
        float lineAlpha = lineMask * u_windLineIntensity * smoothstep(1.0, 20.0, windSpeed);

        // Distance and Depth Fading
        float dist = length(relativePos);
        lineAlpha *= smoothstep(120.0, 30.0, dist); // Distance fade (max visibility at 30m, gone by 120m)
        lineAlpha *= smoothstep(1.0, 5.0, dist);    // Near fade to prevent screen clutter

        // Blend stylized lines onto the scene
        baseColor.rgb = mix(baseColor.rgb, lineColor, lineAlpha);
    }

    FragColor = baseColor;
}
