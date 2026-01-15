#version 430 core

layout (points) in;
layout (triangle_strip, max_vertices = 20) out;

uniform mat4 projection;
uniform mat4 view;

in gl_PerVertex {
    vec4 gl_Position;
} gl_in[];

out vec3 g_Normal;
out vec3 g_Color;

struct Branch {
    vec4 position;
    vec4 parent_position;
    int parent_index;
    float thickness;
    vec2 padding;
};

layout(std430, binding = 1) buffer TreeBranches {
    Branch branches[];
};

void create_cylinder(vec3 start_pos, vec3 end_pos, float radius, int segments) {
    vec3 dir = normalize(end_pos - start_pos);
    vec3 tangent = abs(dir.y) > 0.9 ? vec3(1, 0, 0) : vec3(0, 1, 0);
    vec3 bitangent = normalize(cross(dir, tangent));
    tangent = normalize(cross(bitangent, dir));

    for (int i = 0; i <= segments; ++i) {
        float angle = float(i) / float(segments) * 2.0 * 3.14159;
        vec3 offset = (tangent * cos(angle) + bitangent * sin(angle)) * radius;

        // Bottom vertex
        g_Normal = normalize(offset);
        gl_Position = projection * view * vec4(start_pos + offset, 1.0);
        EmitVertex();

        // Top vertex
        g_Normal = normalize(offset);
        gl_Position = projection * view * vec4(end_pos + offset, 1.0);
        EmitVertex();
    }
    EndPrimitive();
}

void main() {
    g_Color = vec3(0.5, 0.3, 0.1); // Brown color
    Branch b = branches[gl_PrimitiveIDIn];
    if (b.parent_index != -1) {
        create_cylinder(b.parent_position.xyz, b.position.xyz, b.thickness, 8);
    }
}
