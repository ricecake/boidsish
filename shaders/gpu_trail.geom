#version 430 core

// INPUT: Takes 4 adjacent vertices from the Vertex Shader.
// The line segment to be rendered is between gl_in[1] and gl_in[2].
// gl_in[0] and gl_in[3] are used to calculate smooth tangents.
layout(lines_adjacency) in;

// OUTPUT: A single segment of the tube as a triangle strip.
// The number of vertices is small and fixed, avoiding hardware limits.
layout(triangle_strip, max_vertices = 34) out; // (16 segments + 1) * 2 vertices

// From vertex shader
in VS_OUT {
    vec3 position;
    vec3 color;
    float timestamp;
} gs_in[];

// Uniforms
uniform mat4 projection;
uniform mat4 view;
uniform float base_thickness;
uniform float u_current_time;
uniform float u_trail_total_lifespan; // Total time a point stays in the trail

// Outputs to fragment shader
out vec3 vs_frag_pos;
out vec3 vs_normal;
out vec3 vs_color;
out float vs_progress;

// Constants for readability and maintainability
const int TUBE_SEGMENTS = 16;
const float PI = 3.14159265;

// Function to generate a single vertex of the tube's ring.
void generate_vertex(vec3 p, vec3 normal, vec3 binormal, vec3 color, float progress, float angle) {
    // Calculate the final vertex normal on the ring
    vec3 ring_normal = normalize(normal * cos(angle) + binormal * sin(angle));

    // Apply tapering based on the progress (age) of the point
    float taper_scale = 0.2 + 0.8 * progress;

    // Calculate the final vertex position
    vec3 final_pos = p + ring_normal * base_thickness * taper_scale;

    // Set outputs for the fragment shader
    vs_frag_pos = final_pos;
    vs_normal = ring_normal;
    vs_color = color;
    vs_progress = progress;

    // Project to clip space
    gl_Position = projection * view * vec4(final_pos, 1.0);

    EmitVertex();
}

void main() {
    // Extract positions of the 4 adjacent points
    vec3 p0 = gs_in[0].position;
    vec3 p1 = gs_in[1].position;
    vec3 p2 = gs_in[2].position;
    vec3 p3 = gs_in[3].position;

    // Calculate smooth tangents at the segment's start and end points
    // This uses the Catmull-Rom formulation for tangents.
    vec3 tangent1 = normalize(p2 - p0);
    vec3 tangent2 = normalize(p3 - p1);

    // --- Create a stable orientation frame (normal/binormal) ---
    // This robustly handles the case where the tangent is vertical (gimbal lock).
    vec3 up = vec3(0.0, 1.0, 0.0);
    if (abs(dot(up, tangent1)) > 0.999) {
        up = vec3(1.0, 0.0, 0.0); // Use a different 'up' if vertical
    }
    vec3 normal1 = normalize(cross(tangent1, up));
    vec3 binormal1 = normalize(cross(tangent1, normal1));

    // Re-use the same frame for the second point to start, will be corrected if needed
    vec3 normal2 = normal1;
    vec3 binormal2 = binormal1;

    // Check if the second tangent is significantly different
    if (dot(tangent1, tangent2) < 0.999) {
        if (abs(dot(up, tangent2)) > 0.999) {
             up = vec3(1.0, 0.0, 0.0);
        }
        normal2 = normalize(cross(tangent2, up));
        binormal2 = normalize(cross(tangent2, normal2));
    }


    // --- Generate the tube segment as a triangle strip ---
    for (int i = 0; i <= TUBE_SEGMENTS; i++) {
        float angle = 2.0 * PI * float(i) / TUBE_SEGMENTS;

        // Calculate progress (0.0 = oldest, 1.0 = newest)
        float progress1 = 1.0 - clamp((u_current_time - gs_in[1].timestamp) / u_trail_total_lifespan, 0.0, 1.0);
        float progress2 = 1.0 - clamp((u_current_time - gs_in[2].timestamp) / u_trail_total_lifespan, 0.0, 1.0);

        // Generate the two vertices that make up this part of the strip
        generate_vertex(p1, normal1, binormal1, gs_in[1].color, progress1, angle);
        generate_vertex(p2, normal2, binormal2, gs_in[2].color, progress2, angle);
    }

    EndPrimitive();
}
