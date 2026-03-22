#version 460 core

#include "../helpers/voxel_raymarch.glsl"
#include "../lighting.glsl"

in vec3 FragPos;
out vec4 FragColor;

uniform sampler3D u_brickPool;

void main() {
    // Basic single-pass volume rendering
    // We render both sides but only march once from the camera.
    // DISCARD the front faces to avoid rendering twice,
    // ensuring we only trigger the shader once per ray.
    if (gl_FrontFacing) discard;

    vec3 rd = normalize(FragPos - viewPos);
    vec3 ro = viewPos;
    float maxDist = length(FragPos - viewPos);

    // Raymarch through the volume from camera to back face
    vec4 result = raymarch_density(ro, rd, maxDist, u_brickPool);

    float density = result.x;

    // UNMISTAKABLE COLORING
    if (density > 0.0001) {
        // Bright Cyan for detected density
        FragColor = vec4(0.0, 1.0, 1.0, 1.0);
    } else {
        // Semi-transparent Magenta for the proxy boundary
        FragColor = vec4(1.0, 0.0, 1.0, 0.2);
    }
}
