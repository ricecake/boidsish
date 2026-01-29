#version 430 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 Color;
} gs_in[];

out GS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 Color;
} gs_out;

uniform mat4 u_view;
uniform mat4 u_projection;

void emit_shard_vertex(vec3 pos, vec3 norm, vec2 uv, vec4 col) {
    gs_out.FragPos = pos;
    gs_out.Normal = norm;
    gs_out.TexCoords = uv;
    gs_out.Color = col;
    gl_Position = u_projection * u_view * vec4(pos, 1.0);
    EmitVertex();
}

void main() {
    vec3 v0 = gs_in[0].FragPos;
    vec3 v1 = gs_in[1].FragPos;
    vec3 v2 = gs_in[2].FragPos;
    vec3 n = gs_in[0].Normal;
    vec4 color = gs_in[0].Color;

    float side1 = length(v1 - v0);
    float side2 = length(v2 - v1);
    float side3 = length(v0 - v2);
    float thickness = (side1 + side2 + side3) * 0.15;

    vec3 v3 = (v0 + v1 + v2) / 3.0 - n * thickness;
    vec2 t3 = (gs_in[0].TexCoords + gs_in[1].TexCoords + gs_in[2].TexCoords) / 3.0;

    // Face 0: Top
    emit_shard_vertex(v0, n, gs_in[0].TexCoords, color);
    emit_shard_vertex(v1, n, gs_in[1].TexCoords, color);
    emit_shard_vertex(v2, n, gs_in[2].TexCoords, color);
    EndPrimitive();

    // Face 1: Side 1
    vec3 n1 = normalize(cross(v2 - v0, v3 - v0));
    emit_shard_vertex(v0, n1, gs_in[0].TexCoords, color);
    emit_shard_vertex(v2, n1, gs_in[2].TexCoords, color);
    emit_shard_vertex(v3, n1, t3, color);
    EndPrimitive();

    // Face 2: Side 2
    vec3 n2 = normalize(cross(v0 - v1, v3 - v1));
    emit_shard_vertex(v1, n2, gs_in[1].TexCoords, color);
    emit_shard_vertex(v0, n2, gs_in[0].TexCoords, color);
    emit_shard_vertex(v3, n2, t3, color);
    EndPrimitive();

    // Face 3: Side 3
    vec3 n3 = normalize(cross(v1 - v2, v3 - v2));
    emit_shard_vertex(v2, n3, gs_in[2].TexCoords, color);
    emit_shard_vertex(v1, n3, gs_in[1].TexCoords, color);
    emit_shard_vertex(v3, n3, t3, color);
    EndPrimitive();
}
