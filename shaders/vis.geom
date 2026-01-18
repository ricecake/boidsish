#version 420 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in vec3 FragPos[];
in vec3 Normal[];
in vec2 TexCoords[];
in vec3 vs_color[];

out vec3 FragPos_out;
out vec3 Normal_out;
out vec2 TexCoords_out;
out vec3 vs_color_out;
out vec3 barycentric;

void main() {
    for (int i = 0; i < 3; i++) {
        gl_Position = gl_in[i].gl_Position;
        FragPos_out = FragPos[i];
        Normal_out = Normal[i];
        TexCoords_out = TexCoords[i];
        vs_color_out = vs_color[i];
        if (i == 0) barycentric = vec3(1, 0, 0);
        else if (i == 1) barycentric = vec3(0, 1, 0);
        else barycentric = vec3(0, 0, 1);
        EmitVertex();
    }
    EndPrimitive();
}
