#version 330 core
layout (line_strip) in;
layout (triangle_strip, max_vertices = 256) out;

in vec4 gl_in[];

out float Fade;

uniform mat4 projection;
uniform mat4 view;
uniform float thickness = 0.1;

void main() {
    for (int i = 0; i < gl_in.length; i++) {
        vec4 p = gl_in[i].gl_Position;

        vec3 p_world = vec3(p.xyz);
        vec3 p_view = vec3(view * vec4(p_world, 1.0));

        vec3 offset = vec3(0.0, thickness, 0.0);

        gl_Position = projection * vec4(p_view + offset, 1.0);
        Fade = 1.0 - (float(i) / float(gl_in.length));
        EmitVertex();

        gl_Position = projection * vec4(p_view - offset, 1.0);
        Fade = 1.0 - (float(i) / float(gl_in.length));
        EmitVertex();
    }
    EndPrimitive();
}
