#version 430 core

#include "../helpers/voxel_raymarch.glsl"
#include "../lighting.glsl"

in vec3 FragPos;
out vec4 FragColor;

uniform sampler3D u_brickPool;

void main() {
    // Determine ray origin (camera) and direction
    vec3 rd = normalize(FragPos - viewPos);
    vec3 ro = viewPos;

    // Total raymarching distance
    float maxDist = 200.0;

    // Raymarch through the volume
    // We start from the fragment position and march INTO the volume,
    // but better yet, start from the camera if the camera is near/inside.
    vec4 result = raymarch_density(ro, rd, maxDist, u_brickPool);

    float density = result.x;
    if (density <= 0.01) discard;

    // Boost density for visualization
    density *= 5.0;

    // Simple glowing orange for density
    vec3 color = vec3(1.0, 0.6, 0.2) * density;
    FragColor = vec4(color, clamp(density, 0.0, 1.0));
}
