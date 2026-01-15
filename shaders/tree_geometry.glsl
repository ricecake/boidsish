#version 430 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

// Uniforms for transformations
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float scale;

uniform int render_mode; // 0 for branches, 1 for attraction points

// Data from the vertex shader
in VS_OUT {
    vec4 parent_position;
} gs_in[];

// Output to the fragment shader
out vec3 g_Color;

void main() {
    if (render_mode == 0) { // Render branches
        // Get the start and end points of the branch in model space
        vec3 p1_model = gs_in[0].parent_position.xyz;
        vec3 p2_model = gl_in[0].gl_Position.xyz;

        // Don't draw the root branch's connection to its "parent"
        if (gs_in[0].parent_position.w < 0.0) { // Using w component as a flag for the root
            return;
        }

        // Transform direction vectors to view space to make them camera-relative
        vec3 p1_view = (view * model * vec4(p1_model, 1.0)).xyz;
        vec3 p2_view = (view * model * vec4(p2_model, 1.0)).xyz;

        // The direction of the branch in view space
        vec3 dir_view = normalize(p2_view - p1_view);

        // The "up" vector in view space is just (0, 1, 0)
        vec3 up_view = vec3(0, 1, 0);

        // Calculate the vector orthogonal to the branch direction and the up vector (the "side" vector)
        vec3 side_view = normalize(cross(dir_view, up_view));

        // Thickness of the branch
        float thickness = 0.1 * scale;

        // Calculate the four vertices of the quad in view space
        vec3 v0_view = p1_view - side_view * thickness;
        vec3 v1_view = p1_view + side_view * thickness;
        vec3 v2_view = p2_view - side_view * thickness;
        vec3 v3_view = p2_view + side_view * thickness;

        // Project the view-space vertices to clip space and emit them
        gl_Position = projection * vec4(v0_view, 1.0);
        g_Color = vec3(0.5, 0.25, 0.0); // Brown color
        EmitVertex();

        gl_Position = projection * vec4(v1_view, 1.0);
        g_Color = vec3(0.6, 0.3, 0.0); // Slightly lighter brown
        EmitVertex();

        gl_Position = projection * vec4(v2_view, 1.0);
        g_Color = vec3(0.5, 0.25, 0.0);
        EmitVertex();

        gl_Position = projection * vec4(v3_view, 1.0);
        g_Color = vec3(0.6, 0.3, 0.0);
        EmitVertex();

        EndPrimitive();
    } else { // Render attraction points
        vec3 p_model = gl_in[0].gl_Position.xyz;
        vec3 p_view = (view * model * vec4(p_model, 1.0)).xyz;

        float size = 0.2;

        // Camera-facing quad (billboard)
        vec3 up = vec3(0, 1, 0) * size;
        vec3 right = vec3(1, 0, 0) * size;

        gl_Position = projection * vec4(p_view - right - up, 1.0);
        g_Color = vec3(0.0, 1.0, 0.0); // Green
        EmitVertex();

        gl_Position = projection * vec4(p_view + right - up, 1.0);
        g_Color = vec3(0.0, 1.0, 0.0);
        EmitVertex();

        gl_Position = projection * vec4(p_view - right + up, 1.0);
        g_Color = vec3(0.0, 1.0, 0.0);
        EmitVertex();

        gl_Position = projection * vec4(p_view + right + up, 1.0);
        g_Color = vec3(0.0, 1.0, 0.0);
        EmitVertex();

        EndPrimitive();
    }
}
