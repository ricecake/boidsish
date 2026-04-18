#version 460 core

in vec3 vs_frag_pos;
in vec3 vs_normal;
in vec3 vs_color;
in vec2 vs_tex_coords;
in float vs_type;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 VelocityOut;
layout(location = 2) out vec4 NormalOut;
layout(location = 3) out vec4 AlbedoOut;

#include "lighting.glsl"
#include "helpers/lighting.glsl"

uniform bool uIsShadowPass;
uniform mat4 view;

void main() {
    if (uIsShadowPass) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 N = normalize(vs_normal);
    vec3 albedo = vs_color;

    float primaryShadow;
    vec4 litColor = apply_lighting_pbr(vs_frag_pos, N, albedo, 0.5, 0.0, 1.0, primaryShadow);

    FragColor = litColor;
    NormalOut = vec4(normalize(mat3(view) * N), primaryShadow);
    AlbedoOut = vec4(albedo, 1.0);
    VelocityOut = vec4(0.0, 0.0, 0.5, 0.0);
}
