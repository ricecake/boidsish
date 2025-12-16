#version 330 core
layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in VS_OUT {
    vec3 normal;
} gs_in[];

out vec3 fColor;

uniform mat4 view;
uniform mat4 projection;

void main() {
    mat4 pv = projection * view;

    // Edges
    fColor = vec3(1.0, 1.0, 1.0); // White
    for(int i = 0; i < 3; ++i) {
        gl_Position = pv * gl_in[i].gl_Position;
        EmitVertex();
    }
    gl_Position = pv * gl_in[0].gl_Position;
    EmitVertex();
    EndPrimitive();

    // Normals
    fColor = vec3(0.0, 0.0, 1.0); // Blue
    for(int i = 0; i < 3; ++i) {
        vec4 P = gl_in[i].gl_Position;
        vec3 N = normalize(gs_in[i].normal);
        gl_Position = pv * P;
        EmitVertex();
        gl_Position = pv * (P + vec4(N, 0.0) * 0.1);
        EmitVertex();
        EndPrimitive();
    }
}
