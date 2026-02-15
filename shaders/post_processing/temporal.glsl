#ifndef TEMPORAL_GLSL
#define TEMPORAL_GLSL

/**
 * Reusable Temporal Reprojection Library
 */

struct ReprojectionResult {
    vec2 uv;
    bool isValid;
};

ReprojectionResult reproject(vec2 uv, float depth, sampler2D velocityTex) {
    vec2 velocity = texture(velocityTex, uv).rg;
    vec2 prevUV = uv - velocity;

    ReprojectionResult result;
    result.uv = prevUV;
    result.isValid = all(greaterThanEqual(prevUV, vec2(0.0))) && all(lessThanEqual(prevUV, vec2(1.0)));

    return result;
}

// Variance-based clipping for anti-ghosting
vec3 clipHistory(vec3 history, vec3 neighborhoodMin, vec3 neighborhoodMax) {
    return clamp(history, neighborhoodMin, neighborhoodMax);
}

// Advanced neighborhood clamping using 3x3 or 5x5
void getNeighborhoodStats(sampler2D tex, vec2 uv, out vec3 nMin, out vec3 nMax, out vec3 nAvg) {
    vec2 texelSize = 1.0 / textureSize(tex, 0);
    nMin = vec3(10000.0);
    nMax = vec3(-10000.0);
    nAvg = vec3(0.0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec3 sampleColor = texture(tex, uv + vec2(x, y) * texelSize).rgb;
            nMin = min(nMin, sampleColor);
            nMax = max(nMax, sampleColor);
            nAvg += sampleColor;
        }
    }
    nAvg /= 9.0;
}

#endif // TEMPORAL_GLSL
