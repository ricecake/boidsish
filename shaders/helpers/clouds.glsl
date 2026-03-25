#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "fast_noise.glsl"

// Cloud density calculation helper
// Returns a density value [0, 1+] based on world-space position
float calculateCloudDensity(vec3 p, float cloudAltitude, float cloudThickness, float cloudDensityBase, float worldScale, float time) {
    float scaledCloudAltitude = cloudAltitude * worldScale;
    float scaledCloudThickness = cloudThickness * worldScale;

    // Height-based tapering
    float h = (p.y - scaledCloudAltitude) / max(scaledCloudThickness, 0.001);
    float tapering = smoothstep(0.0, 0.1, h) * smoothstep(1.0, 0.9, h);
    if (tapering <= 0.01) return 0.0;

    // Weather map for large-scale variation
    float weatherMap = fastWorley3d(vec3(p.xz / (4000.0 * worldScale), time * 0.01));
    float workingCloudDensity = cloudDensityBase + 5.0 * weatherMap;

    // Detail noise
    vec3 p_noise = p + 2.0 * fastCurl3d(vec3(p.xz / 500.0, time / 60.0));
    vec3 p_scaled = p_noise / (1000.0 * worldScale);

    float noise = fastWorley3d(vec3(p_scaled.xz, p_scaled.y + time * 0.01));
    float erosion = fastRidge3d(p_noise / (600.0 * worldScale)) * 0.5 + 0.5;

    // Remap noise with erosion
    noise = (noise - (1.0 - erosion)) / max(0.0001, 1.0 - (1.0 - erosion));
    noise = clamp(noise, 0.0, 1.0);

    return smoothstep(0.2, 0.6, noise) * workingCloudDensity * tapering;
}

#endif // HELPERS_CLOUDS_GLSL
