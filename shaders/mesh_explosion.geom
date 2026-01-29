#version 430 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 12) out;

in vec3 vWorldPos[];
in vec3 vNormal[];
in vec2 vTexCoords[];
in vec4 vColor[];
in vec3 vBaryThickness[];

out vec3 fFragPos;
out vec3 fNormal;
out vec2 fTexCoords;
out vec4 fColor;

uniform mat4 u_view;
uniform mat4 u_projection;

void emit_shard_vertex(vec3 pos, vec3 norm, vec2 uv, vec4 col) {
    fFragPos = pos;
    fNormal = norm;
    fTexCoords = uv;
    fColor = col;
    gl_Position = u_projection * u_view * vec4(pos, 1.0);
    EmitVertex();
}

void main() {
    // If the fragment is inactive, vColor[0].a will be 0 and gl_Position was out of bounds
    if (vColor[0].a <= 0.0) return;

    vec3 v0 = vWorldPos[0];
    vec3 v1 = vWorldPos[1];
    vec3 v2 = vWorldPos[2];

    // Calculate face normal
    vec3 n = normalize(cross(v1 - v0, v2 - v0));

    // Random barycentric extrusion point
    float u = vBaryThickness[0].x;
    float v = vBaryThickness[0].y;
    float w = 1.0 - u - v;
    vec3 base_point = v0 * u + v1 * v + v2 * w;

    // Thickness based on triangle size and random scale
    float edge0 = length(v1 - v0);
    float edge1 = length(v2 - v1);
    float edge2 = length(v0 - v2);
    float avg_edge = (edge0 + edge1 + edge2) / 3.0;

    float thickness = avg_edge * 0.4 * vBaryThickness[0].z;

    // Apex of the tetrahedron (extrude along negative normal)
    vec3 apex = base_point - n * thickness;

    vec4 color = vColor[0];

    // Face 0: Top (Original Triangle)
    emit_shard_vertex(v0, n, vTexCoords[0], color);
    emit_shard_vertex(v1, n, vTexCoords[1], color);
    emit_shard_vertex(v2, n, vTexCoords[2], color);
    EndPrimitive();

    // For side faces, we need to ensure normals point outward.
    // The base triangle v0, v1, v2 is CCW from outside.
    // So the side faces should be:
    // Face 1: v1, v0, apex
    // Face 2: v2, v1, apex
    // Face 3: v0, v2, apex

    vec2 apexUV = vTexCoords[0] * u + vTexCoords[1] * v + vTexCoords[2] * w;

    // Face 1: Side 1-0-Apex
    vec3 n1 = normalize(cross(v0 - v1, apex - v1));
    emit_shard_vertex(v1, n1, vTexCoords[1], color);
    emit_shard_vertex(v0, n1, vTexCoords[0], color);
    emit_shard_vertex(apex, n1, apexUV, color);
    EndPrimitive();

    // Face 2: Side 2-1-Apex
    vec3 n2 = normalize(cross(v1 - v2, apex - v2));
    emit_shard_vertex(v2, n2, vTexCoords[2], color);
    emit_shard_vertex(v1, n2, vTexCoords[1], color);
    emit_shard_vertex(apex, n2, apexUV, color);
    EndPrimitive();

    // Face 3: Side 0-2-Apex
    vec3 n3 = normalize(cross(v2 - v0, apex - v0));
    emit_shard_vertex(v0, n3, vTexCoords[0], color);
    emit_shard_vertex(v2, n3, vTexCoords[2], color);
    emit_shard_vertex(apex, n3, apexUV, color);
    EndPrimitive();
}
