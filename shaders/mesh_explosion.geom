#version 430 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 Color;
} gs_in[];

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 Color;

uniform mat4 u_view;
uniform mat4 u_projection;

void main() {
    vec3 v0 = gs_in[0].FragPos;
    vec3 v1 = gs_in[1].FragPos;
    vec3 v2 = gs_in[2].FragPos;
    vec3 n = gs_in[0].Normal; // Assuming all vertices of the triangle have same normal from VS
    vec4 color = gs_in[0].Color;

    // Calculate thickness based on triangle size
    float side1 = length(v1 - v0);
    float side2 = length(v2 - v1);
    float side3 = length(v0 - v2);
    float thickness = (side1 + side2 + side3) * 0.15;

    // The 4th vertex (apex of the tetrahedron)
    // We move it in the negative normal direction (inwards)
    vec3 v3 = (v0 + v1 + v2) / 3.0 - n * thickness;

    // Common TexCoords for the apex
    vec2 t3 = (gs_in[0].TexCoords + gs_in[1].TexCoords + gs_in[2].TexCoords) / 3.0;

    // Face 0: Top (the original triangle)
    Normal = n;
    Color = color;

    FragPos = v0; TexCoords = gs_in[0].TexCoords; gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    FragPos = v1; TexCoords = gs_in[1].TexCoords; gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    FragPos = v2; TexCoords = gs_in[2].TexCoords; gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    EndPrimitive();

    // Face 1: Side 1 (v0, v2, v3)
    Normal = normalize(cross(v2 - v0, v3 - v0));
    FragPos = v0; TexCoords = gs_in[0].TexCoords; gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    FragPos = v2; TexCoords = gs_in[2].TexCoords; gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    FragPos = v3; TexCoords = t3;                 gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    EndPrimitive();

    // Face 2: Side 2 (v1, v0, v3)
    Normal = normalize(cross(v0 - v1, v3 - v1));
    FragPos = v1; TexCoords = gs_in[1].TexCoords; gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    FragPos = v0; TexCoords = gs_in[0].TexCoords; gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    FragPos = v3; TexCoords = t3;                 gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    EndPrimitive();

    // Face 3: Side 3 (v2, v1, v3)
    Normal = normalize(cross(v1 - v2, v3 - v2));
    FragPos = v2; TexCoords = gs_in[2].TexCoords; gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    FragPos = v1; TexCoords = gs_in[1].TexCoords; gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    FragPos = v3; TexCoords = t3;                 gl_Position = u_projection * u_view * vec4(FragPos, 1.0); EmitVertex();
    EndPrimitive();
}
