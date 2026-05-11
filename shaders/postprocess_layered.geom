#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;

in vec2 TexCoords[];
out vec2 vTexCoords;

void main() {
    for (int layer = 0; layer < 2; layer++) {
        gl_Layer = layer;
        for (int i = 0; i < 3; i++) {
            vTexCoords = TexCoords[i];
            gl_Position = gl_in[i].gl_Position;
            EmitVertex();
        }
        EndPrimitive();
    }
}
