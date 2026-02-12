#ifndef BLUE_NOISE_GLSL
#define BLUE_NOISE_GLSL

// Procedural Blue Noise approximation
// Based on: https://www.shadertoy.com/view/3tB3z3
float blueNoise(vec2 p) {
    const vec3 a = vec3(0.1031, 0.1030, 0.0973);
    vec3 p3 = fract(vec3(p.xyx) * a);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Temporal blue noise
float blueNoise(vec2 p, float time) {
    return fract(blueNoise(p) + time);
}

// Map value to -0.5 to 0.5 range
float blueNoiseM(vec2 p, float time) {
    return blueNoise(p, time) - 0.5;
}

#endif
