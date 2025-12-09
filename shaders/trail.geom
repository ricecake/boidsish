#version 330 core
layout (lines) in;
layout (triangle_strip, max_vertices = 4) out;

in float Progress[];

out float Fade;

uniform mat4 projection;
uniform mat4 view;
uniform float thickness;

void main() {
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;

    vec3 p0_world = vec3(p0.xyz);
    vec3 p1_world = vec3(p1.xyz);

    vec3 p0_view = vec3(view * vec4(p0_world, 1.0));
    vec3 p1_view = vec3(view * vec4(p1_world, 1.0));

    vec3 line_dir = normalize(p1_view - p0_view);
    vec3 offset = normalize(cross(line_dir, vec3(0.0, 0.0, -1.0))) * thickness;

    gl_Position = projection * vec4(p0_view + offset, 1.0);
    Fade = Progress[0];
    EmitVertex();

    gl_Position = projection * vec4(p0_view - offset, 1.0);
    Fade = Progress[0];
    EmitVertex();

    gl_Position = projection * vec4(p1_view + offset, 1.0);
    Fade = Progress[1];
    EmitVertex();

    gl_Position = projection * vec4(p1_view - offset, 1.0);
    Fade = Progress[1];
    EmitVertex();

    EndPrimitive();
}
