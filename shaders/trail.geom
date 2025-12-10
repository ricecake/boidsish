#version 330 core
layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = 4) out;

in vec3 vs_color[];
in float vs_progress[];

out vec3 color;
out float fade;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 projection;
uniform mat4 view;
uniform float thickness;

void main() {
    // Transform all points to clip space
    vec4 p0 = projection * view * gl_in[0].gl_Position;
    vec4 p1 = projection * view * gl_in[1].gl_Position;
    vec4 p2 = projection * view * gl_in[2].gl_Position;
    vec4 p3 = projection * view * gl_in[3].gl_Position;

    // Get screen-space coordinates by perspective division
    vec2 p0_ss = p0.xy / p0.w;
    vec2 p1_ss = p1.xy / p1.w;
    vec2 p2_ss = p2.xy / p2.w;
    vec2 p3_ss = p3.xy / p3.w;

    // Screen-space directions and normals
    vec2 dir_curr = normalize(p2_ss - p1_ss);
    vec2 normal_curr = vec2(-dir_curr.y, dir_curr.x);

    vec2 miter1, miter2;

    // Miter at the start of the segment (p1). Compare world-space positions.
    if (distance(gl_in[0].gl_Position, gl_in[1].gl_Position) < 0.0001) {
        miter1 = normal_curr;
    } else {
        vec2 dir_prev = normalize(p1_ss - p0_ss);
        vec2 normal_prev = vec2(-dir_prev.y, dir_prev.x);
        miter1 = normalize(normal_prev + normal_curr);
    }

    // Miter at the end of the segment (p2). Compare world-space positions.
    if (distance(gl_in[2].gl_Position, gl_in[3].gl_Position) < 0.0001) {
        miter2 = normal_curr;
    } else {
        vec2 dir_next = normalize(p3_ss - p2_ss);
        vec2 normal_next = vec2(-dir_next.y, dir_next.x);
        miter2 = normalize(normal_curr + normal_next);
    }

    // To get a constant thickness in screen space, we offset in screen space (by `thickness`)
    // and apply that offset back in clip space, scaled by the w-component.
    vec2 offset1 = miter1 * thickness;
    vec2 offset2 = miter2 * thickness;

    // We also need view-space position and normal for lighting calculations
    vec4 p1_view = view * gl_in[1].gl_Position;
    vec4 p2_view = view * gl_in[2].gl_Position;
    vec3 view_normal = vec3(0, 0, 1); // Ribbon always faces camera

    // First point of the segment
    gl_Position = p1 + vec4(offset1 * p1.w, 0.0, 0.0);
    color = vs_color[1];
    fade = vs_progress[1];
    FragPos = p1_view.xyz;
    Normal = view_normal;
    EmitVertex();

    gl_Position = p1 - vec4(offset1 * p1.w, 0.0, 0.0);
    color = vs_color[1];
    fade = vs_progress[1];
    FragPos = p1_view.xyz;
    Normal = view_normal;
    EmitVertex();

    // Second point of the segment
    gl_Position = p2 + vec4(offset2 * p2.w, 0.0, 0.0);
    color = vs_color[2];
    fade = vs_progress[2];
    FragPos = p2_view.xyz;
    Normal = view_normal;
    EmitVertex();

    gl_Position = p2 - vec4(offset2 * p2.w, 0.0, 0.0);
    color = vs_color[2];
    fade = vs_progress[2];
    FragPos = p2_view.xyz;
    Normal = view_normal;
    EmitVertex();

    EndPrimitive();
}
