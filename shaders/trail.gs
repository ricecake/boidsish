#version 330 core

layout (line_strip_adjacency) in;
layout (triangle_strip, max_vertices = 256) out;

in VS_OUT {
    vec4 color;
} gs_in[];

out vec4 fColor;

uniform float thickness;

void main() {
    vec3 p1 = gl_in[1].gl_Position.xyz / gl_in[1].gl_Position.w;
    vec3 p2 = gl_in[2].gl_Position.xyz / gl_in[2].gl_Position.w;

    vec2 dir = normalize(p2.xy - p1.xy);
    vec2 offset = vec2(-dir.y, dir.x) * thickness;

    fColor = gs_in[1].color;
    gl_Position = vec4(p1.xy + offset, p1.z, 1.0);
    EmitVertex();

    fColor = gs_in[1].color;
    gl_Position = vec4(p1.xy - offset, p1.z, 1.0);
    EmitVertex();

    fColor = gs_in[2].color;
    gl_Position = vec4(p2.xy + offset, p2.z, 1.0);
    EmitVertex();

    fColor = gs_in[2].color;
    gl_Position = vec4(p2.xy - offset, p2.z, 1.0);
    EmitVertex();

    EndPrimitive();
}
