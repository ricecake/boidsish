#version 330 core

layout (line_strip_adjacency) in;
layout (triangle_strip, max_vertices = 4) out;

in VS_OUT {
    vec4 color;
} gs_in[];

out vec4 fColor;

uniform float thickness;

void main() {
    vec4 p1 = gl_in[1].gl_Position;
    vec4 p2 = gl_in[2].gl_Position;

    vec3 ndc1 = p1.xyz / p1.w;
    vec3 ndc2 = p2.xyz / p2.w;

    vec2 dir = normalize(ndc2.xy - ndc1.xy);
    vec2 offset = vec2(-dir.y, dir.x) * thickness * 0.1;

    fColor = gs_in[1].color;
    gl_Position = vec4((ndc1.xy - offset) * p1.w, ndc1.z * p1.w, p1.w);
    EmitVertex();

    gl_Position = vec4((ndc1.xy + offset) * p1.w, ndc1.z * p1.w, p1.w);
    EmitVertex();

    fColor = gs_in[2].color;
    gl_Position = vec4((ndc2.xy - offset) * p2.w, ndc2.z * p2.w, p2.w);
    EmitVertex();

    gl_Position = vec4((ndc2.xy + offset) * p2.w, ndc2.z * p2.w, p2.w);
    EmitVertex();

    EndPrimitive();
}
