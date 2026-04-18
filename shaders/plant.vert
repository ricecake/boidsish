#version 460 core

layout(location = 0) in vec4 aPos;    // xyz: pos, w: u
layout(location = 1) in vec4 aNormal; // xyz: normal, w: v
layout(location = 2) in vec4 aColor;  // rgb: color, w: type

out vec3 vs_frag_pos;
out vec3 vs_normal;
out vec3 vs_color;
out vec2 vs_tex_coords;
out float vs_type;

uniform mat4 view;
uniform mat4 projection;
uniform bool uIsShadowPass;

#include "helpers/lighting.glsl"

void main() {
    vs_frag_pos = aPos.xyz;
    vs_normal = aNormal.xyz;
    vs_color = aColor.rgb;
    vs_tex_coords = vec2(aPos.w, aNormal.w);
    vs_type = aColor.w;

    gl_Position = projection * view * vec4(vs_frag_pos, 1.0);
}
