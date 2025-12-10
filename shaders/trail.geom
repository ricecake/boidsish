#version 330 core
layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = 10) out;

in vec3 vs_color[];
in float vs_progress[];

out vec3 color;
out float fade;

uniform mat4 projection;
uniform mat4 view;
uniform float thickness;

// Helper to calculate the frame (right and up vectors) for a given direction
void GetFrame(vec3 dir, out vec3 right, out vec3 up) {
    vec3 world_up = vec3(0.0, 1.0, 0.0);
    if (abs(dot(dir, world_up)) > 0.99) {
        world_up = vec3(1.0, 0.0, 0.0); // Use a different up vector if parallel
    }
    right = normalize(cross(dir, world_up));
    up = normalize(cross(right, dir));
}

void main() {
    vec3 p0 = gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz;
    vec3 p3 = gl_in[3].gl_Position.xyz;

    vec3 c1 = vs_color[1];
    vec3 c2 = vs_color[2];
    float prog1 = vs_progress[1];
    float prog2 = vs_progress[2];

    // Get directions
    vec3 dir_curr = normalize(p2 - p1);

    vec3 right1, up1, right2, up2;

    // Frame at the start of the segment (p1)
    if (distance(p0, p1) < 0.0001) { // Start of the entire trail
        GetFrame(dir_curr, right1, up1);
    } else { // Miter join
        vec3 dir_prev = normalize(p1 - p0);
        vec3 r_prev, u_prev, r_curr, u_curr;
        GetFrame(dir_prev, r_prev, u_prev);
        GetFrame(dir_curr, r_curr, u_curr);
        right1 = normalize(r_prev + r_curr);
        up1 = normalize(u_prev + u_curr);
    }

    // Frame at the end of the segment (p2)
    if (distance(p2, p3) < 0.0001) { // End of the entire trail
        GetFrame(dir_curr, right2, up2);
    } else { // Miter join
        vec3 dir_next = normalize(p3 - p2);
        vec3 r_curr, u_curr, r_next, u_next;
        GetFrame(dir_curr, r_curr, u_curr);
        GetFrame(dir_next, r_next, u_next);
        right2 = normalize(r_curr + r_next);
        up2 = normalize(u_curr + u_next);
    }

    // Calculate the 4 corner vertices for the start and end points
    vec3 p1_br = p1 + (right1 * thickness - up1 * thickness);
    vec3 p1_bl = p1 + (-right1 * thickness - up1 * thickness);
    vec3 p1_tr = p1 + (right1 * thickness + up1 * thickness);
    vec3 p1_tl = p1 + (-right1 * thickness + up1 * thickness);

    vec3 p2_br = p2 + (right2 * thickness - up2 * thickness);
    vec3 p2_bl = p2 + (-right2 * thickness - up2 * thickness);
    vec3 p2_tr = p2 + (right2 * thickness + up2 * thickness);
    vec3 p2_tl = p2 + (-right2 * thickness + up2 * thickness);

    // Emit a triangle strip that connects the start and end quads
    gl_Position = projection * view * vec4(p1_br, 1.0); color = c1; fade = prog1; EmitVertex();
    gl_Position = projection * view * vec4(p2_br, 1.0); color = c2; fade = prog2; EmitVertex();
    gl_Position = projection * view * vec4(p1_tr, 1.0); color = c1; fade = prog1; EmitVertex();
    gl_Position = projection * view * vec4(p2_tr, 1.0); color = c2; fade = prog2; EmitVertex();
    gl_Position = projection * view * vec4(p1_tl, 1.0); color = c1; fade = prog1; EmitVertex();
    gl_Position = projection * view * vec4(p2_tl, 1.0); color = c2; fade = prog2; EmitVertex();
    gl_Position = projection * view * vec4(p1_bl, 1.0); color = c1; fade = prog1; EmitVertex();
    gl_Position = projection * view * vec4(p2_bl, 1.0); color = c2; fade = prog2; EmitVertex();
    gl_Position = projection * view * vec4(p1_br, 1.0); color = c1; fade = prog1; EmitVertex();
    gl_Position = projection * view * vec4(p2_br, 1.0); color = c2; fade = prog2; EmitVertex();

    EndPrimitive();
}
