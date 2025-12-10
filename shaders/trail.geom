#version 330 core
layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = 4) out;

in vec3 vs_color[];
in float vs_progress[];

out vec3 FragPos;
out vec3 Normal;
out vec3 color;

uniform mat4 projection;
uniform mat4 view;
uniform float thickness;

void main() {
    mat4 pv = projection * view;

    // Transform all points to clip space
    vec4 p0 = pv * gl_in[0].gl_Position;
    vec4 p1 = pv * gl_in[1].gl_Position;
    vec4 p2 = pv * gl_in[2].gl_Position;
    vec4 p3 = pv * gl_in[3].gl_Position;

    // De-homogenize (perspective divide)
    p0 /= p0.w;
    p1 /= p1.w;
    p2 /= p2.w;
    p3 /= p3.w;

    // Screen-space directions and normals
    vec2 dir_curr = normalize(p2.xy - p1.xy);
    vec2 normal_curr = vec2(-dir_curr.y, dir_curr.x);

    vec2 miter1, miter2;

    // Miter at the start of the segment (p1)
    if (distance(p0, p1) < 0.0001) { // Start of the line, use normal_curr
        miter1 = normal_curr;
    } else {
        vec2 dir_prev = normalize(p1.xy - p0.xy);
        vec2 normal_prev = vec2(-dir_prev.y, dir_prev.x);
        miter1 = normalize(normal_prev + normal_curr);
    }

    // Miter at the end of the segment (p2)
    if (distance(p2, p3) < 0.0001) { // End of the line, use normal_curr
        miter2 = normal_curr;
    } else {
        vec2 dir_next = normalize(p3.xy - p2.xy);
        vec2 normal_next = vec2(-dir_next.y, dir_next.x);
        miter2 = normalize(normal_curr + normal_next);
    }

    // Calculate the world positions for lighting
    vec3 worldPos1 = gl_in[1].gl_Position.xyz;
    vec3 worldPos2 = gl_in[2].gl_Position.xyz;
    vec3 worldNormal = normalize(cross(worldPos2 - worldPos1, vec3(0,1,0))); // Approximation

    // First point of the segment
    gl_Position = p1 + vec4(miter1 * thickness, 0.0, 0.0);
    FragPos = worldPos1;
    Normal = worldNormal;
    color = vs_color[1];
    EmitVertex();

    gl_Position = p1 - vec4(miter1 * thickness, 0.0, 0.0);
    FragPos = worldPos1;
    Normal = worldNormal;
    color = vs_color[1];
    EmitVertex();

    // Second point of the segment
    gl_Position = p2 + vec4(miter2 * thickness, 0.0, 0.0);
    FragPos = worldPos2;
    Normal = worldNormal;
    color = vs_color[2];
    EmitVertex();

    gl_Position = p2 - vec4(miter2 * thickness, 0.0, 0.0);
    FragPos = worldPos2;
    Normal = worldNormal;
    color = vs_color[2];
    EmitVertex();

    EndPrimitive();
}
