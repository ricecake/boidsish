#version 460 core

#include "../helpers/wind.glsl"
#include "../helpers/fast_noise.glsl"
#include "../types/temporal_data.glsl"

in vec2 vTexCoords;
out vec4 FragColor;

uniform sampler2D u_screenTexture;
uniform sampler2D u_depthTexture;

uniform float u_time;
uniform float u_windLineIntensity;
uniform float u_heatShimmerIntensity;

vec3 worldPosFromDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    vec4 clipPos = vec4(vTexCoords * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = invProjection * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = invView * viewPos;
    return worldPos.xyz;
}

void main() {
    float depth = texture(u_depthTexture, vTexCoords).r;

    // Background handling (infinity)
    if (depth >= 1.0) {
        FragColor = texture(u_screenTexture, vTexCoords);
        return;
    }

    vec3 worldPos = worldPosFromDepth(depth);

    // 1. Heat Shimmer / Refraction
    vec4 scalars = getWeatherScalarsAtPosition(worldPos);
    float temperature = scalars.x; // Kelvin

    vec2 distortedUV = vTexCoords;
    float shimmerFactor = smoothstep(310.0, 340.0, temperature) * u_heatShimmerIntensity;

    if (shimmerFactor > 0.0) {
        float noise = fastSimplex3d(vec3(worldPos.xz * 2.0, u_time * 10.0)) * 0.5 + 0.5;
        distortedUV += (noise - 0.5) * 0.01 * shimmerFactor;
    }

    vec4 baseColor = texture(u_screenTexture, distortedUV);

    // 2. Stylized Wind Lines
    vec3 wind = getWindAtPosition(worldPos);
    float windSpeed = length(wind);

    if (windSpeed > 0.1 && u_windLineIntensity > 0.0) {
        vec3 windDir = wind / windSpeed;

        // Advect noise along wind direction
        float lineNoise = fastSimplex3d(worldPos * 0.5 - windDir * u_time * 5.0);
        lineNoise = smoothstep(0.7, 0.95, lineNoise);

        // Faint stylized lines
        vec3 lineColor = vec3(0.9, 0.95, 1.0);
        float lineAlpha = lineNoise * u_windLineIntensity * smoothstep(0.1, 10.0, windSpeed);

        // Distance fade
        float dist = length(worldPos - invView[3].xyz);
        lineAlpha *= smoothstep(100.0, 20.0, dist);

        baseColor.rgb = mix(baseColor.rgb, lineColor, lineAlpha);
    }

    FragColor = baseColor;
}
