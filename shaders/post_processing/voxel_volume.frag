#version 430 core

#include "../helpers/voxel_raymarch.glsl"

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D u_inputTexture;
uniform sampler2D u_depthTexture;

uniform float u_stepSize;
uniform float u_maxDistance;
uniform float u_densityScale;
uniform vec3  u_ambientColor;
uniform uint  u_styleMask;

uniform vec3  u_cameraPos;
uniform mat4  u_invProj;
uniform mat4  u_invView;

// --- Reconstruction from Depth ---

vec3 reconstruct_world_pos(vec2 uv, float depth) {
    vec4 clip_space = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view_space = u_invProj * clip_space;
    view_space /= view_space.w;
    vec4 world_space = u_invView * view_space;
    return world_space.xyz;
}

void main() {
    vec3 scene_color = texture(u_inputTexture, TexCoords).rgb;
    float depth = texture(u_depthTexture, TexCoords).r;

    vec3 world_pos = reconstruct_world_pos(TexCoords, depth);
    vec3 ray_dir = normalize(world_pos - u_cameraPos);
    float scene_dist = length(world_pos - u_cameraPos);

    Ray ray;
    ray.origin = u_cameraPos;
    ray.direction = ray_dir;

    // March through voxels
    RaymarchResult res = march_voxels(ray, min(u_maxDistance, scene_dist), u_stepSize);

    if (res.density > 0.0) {
        float alpha = clamp(res.density * u_densityScale, 0.0, 1.0);

        // Color by direction
        vec3 voxel_color = abs(res.direction);

        vec3 final_color = mix(scene_color, voxel_color + u_ambientColor, alpha);
        FragColor = vec4(final_color, 1.0);
    } else {
        FragColor = vec4(scene_color, 1.0);
    }
}
