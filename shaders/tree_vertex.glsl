#version 430 core

// Per-vertex attributes from VBO
layout (location = 0) in vec4 a_Position; // The position of the current branch

// Per-instance attributes from SSBO
struct Branch {
    vec4 position;
    vec4 parent_position;
    int parent_index;
    float thickness;
    vec2 padding;
};

// Corrected syntax: 'readonly' comes before 'buffer'
layout(std430, binding = 1) readonly buffer TreeBranches {
    Branch tree_branches[];
};

// Uniform to determine what we're rendering
uniform int render_mode; // 0 for branches, 1 for attraction points

// Outputs to the geometry shader
out VS_OUT {
    vec4 parent_position;
    float thickness;
} vs_out;

void main() {
    // Pass the model-space position of the current vertex to the geometry shader.
    // The transformation to clip space will happen there.
    gl_Position = a_Position;

    if (render_mode == 0) {
        // Find the parent position from the buffer and pass it to the geometry shader.
        // gl_VertexID is the index of the current vertex being processed.
        vs_out.parent_position = tree_branches[gl_VertexID].parent_position;
        vs_out.thickness = tree_branches[gl_VertexID].thickness;
    } else {
        // Attraction points don't have parents
        vs_out.parent_position = vec4(0.0);
        vs_out.thickness = 0.0;
    }
}
