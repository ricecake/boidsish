#version 460 core

#include "../helpers/voxel_raymarch.glsl"
#include "../lighting.glsl"

in vec3 FragPos;
out vec4 FragColor;

uniform sampler3D u_brickPool;

void main() {
    // Determine ray origin (camera) and direction
    vec3 rd = normalize(FragPos - viewPos);
    vec3 ro = viewPos;

    // Start raymarching from the proxy surface if the camera is outside
    // For simplicity, start from viewPos and assume maxDist is sufficient
    float maxDist = 800.0;

    // Raymarch through the volume
    vec4 result = raymarch_density(ro, rd, maxDist, u_brickPool);

    float density = result.x;

    // UNMISTAKABLE COLORING
    if (density > 0.0001) {
        // Vibrant Neon Green for density
        vec3 color = mix(vec3(0.1, 1.0, 0.2), vec3(1.0, 1.0, 1.0), clamp(density * 0.1, 0.0, 1.0));
        FragColor = vec4(color, 0.9);
    } else {
        // Semi-transparent hot pink for volume bounds (very visible)
        FragColor = vec4(1.0, 0.0, 0.5, 0.1);
    }
}
