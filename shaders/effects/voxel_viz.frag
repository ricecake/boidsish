#version 430 core

#include "../helpers/voxel_raymarch.glsl"
#include "../lighting.glsl"

in vec3 FragPos;
out vec4 FragColor;

uniform sampler3D u_brickPool;

void main() {
    vec3 ro = viewPos;
    vec3 rd = normalize(FragPos - viewPos);
    float maxDist = length(FragPos - viewPos);

    // Raymarch through the volume
    vec4 result = raymarch_density(ro, rd, maxDist, u_brickPool);

    float density = result.x;
    if (density <= 0.0) discard;

    // Simple glowing orange for density
    vec3 color = vec3(1.0, 0.6, 0.2) * density;
    FragColor = vec4(color, clamp(density, 0.0, 1.0));
}
