#version 330 core
layout (line_strip) in;
layout (triangle_strip, max_vertices = 10) out;

in vec3 vs_color[];
in float vs_progress[];

out vec3 color;
out float fade;

uniform mat4 projection;
uniform mat4 view;
uniform float thickness;

void main() {
    vec4 p_start = gl_in[0].gl_Position;
    vec4 p_end = gl_in[1].gl_Position;

    vec3 dir = normalize(vec3(p_end - p_start));

    vec3 c_start = vs_color[0];
    vec3 c_end = vs_color[1];

    float prog_start = vs_progress[0];
    float prog_end = vs_progress[1];

    vec3 up = vec3(0.0, 1.0, 0.0);
    // Avoid gimbal lock
    if (abs(dot(dir, up)) > 0.99) {
        up = vec3(1.0, 0.0, 0.0);
    }

    vec3 right = normalize(cross(dir, up));
    up = normalize(cross(right, dir));

    // Calculate the 4 corner vertices for the start and end points
    vec4 s_tr = p_start + vec4(right * thickness + up * thickness, 0.0);
    vec4 s_tl = p_start + vec4(-right * thickness + up * thickness, 0.0);
    vec4 s_bl = p_start + vec4(-right * thickness - up * thickness, 0.0);
    vec4 s_br = p_start + vec4(right * thickness - up * thickness, 0.0);

    vec4 e_tr = p_end + vec4(right * thickness + up * thickness, 0.0);
    vec4 e_tl = p_end + vec4(-right * thickness + up * thickness, 0.0);
    vec4 e_bl = p_end + vec4(-right * thickness - up * thickness, 0.0);
    vec4 e_br = p_end + vec4(right * thickness - up * thickness, 0.0);

    // Emit a triangle strip that connects the start and end quads
    // This creates the 4 sides of the tube segment

    // Side 1: Bottom
    gl_Position = projection * view * s_br;
    color = c_start;
    fade = prog_start;
    EmitVertex();
    gl_Position = projection * view * e_br;
    color = c_end;
    fade = prog_end;
    EmitVertex();

    // Side 2: Right
    gl_Position = projection * view * s_tr;
    color = c_start;
    fade = prog_start;
    EmitVertex();
    gl_Position = projection * view * e_tr;
    color = c_end;
    fade = prog_end;
    EmitVertex();

    // Side 3: Top
    gl_Position = projection * view * s_tl;
    color = c_start;
    fade = prog_start;
    EmitVertex();
    gl_Position = projection * view * e_tl;
    color = c_end;
    fade = prog_end;
    EmitVertex();

    // Side 4: Left
    gl_Position = projection * view * s_bl;
    color = c_start;
    fade = prog_start;
    EmitVertex();
    gl_Position = projection * view * e_bl;
    color = c_end;
    fade = prog_end;
    EmitVertex();

    // Close the loop back to the bottom side
    gl_Position = projection * view * s_br;
    color = c_start;
    fade = prog_start;
    EmitVertex();
    gl_Position = projection * view * e_br;
    color = c_end;
    fade = prog_end;
    EmitVertex();

    EndPrimitive();
}
