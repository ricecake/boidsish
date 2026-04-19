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
uniform vec4 clipPlane;
uniform float uTime;
uniform bool uIsShadowPass;

#include "helpers/lighting.glsl"
#include "temporal_data.glsl"

out vec4 CurPosition;
out vec4 PrevPosition;

void main() {
    vec3 localPos = aPos.xyz;
    float t = aNormal.w; // along the tube (0 to 1)

    // Micro-sway for the tube itself
    if (!uIsShadowPass) {
        float sway = sin(uTime * 2.0 + t * 5.0) * 0.05 * t * t;
        localPos.x += sway;
    }

    vs_frag_pos = localPos;
    vs_normal = aNormal.xyz;
    vs_color = aColor.rgb;
    vs_tex_coords = vec2(aPos.w, aNormal.w);
    vs_type = aColor.w;

    gl_Position = projection * view * vec4(vs_frag_pos, 1.0);
    gl_ClipDistance[0] = dot(vec4(vs_frag_pos, 1.0), clipPlane);
    CurPosition = gl_Position;
    PrevPosition = prevViewProjection * vec4(vs_frag_pos, 1.0);
}
