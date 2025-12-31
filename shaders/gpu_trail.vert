#version 430 core

// Input: An index pointing to a specific trail point in the SSBO.
// This is provided by the C++ side via a VBO/EBO for an adjacency primitive.
layout (location = 0) in int in_point_storage_index;

// SSBO matching the layout defined in the compute shader and C++ host code.
struct TrailPoint {
    vec4 position;
    vec4 color;
    float timestamp;
    float unused1, unused2, unused3;
};

layout(std430, binding = 0) readonly buffer TrailDataBuffer {
    TrailPoint points[];
};

// Outputs sent to the Geometry Shader stage.
// An interface block is used to group them.
out VS_OUT {
    vec3 position;
    vec3 color;
    float timestamp;
} gs_in;

void main() {
    // Fetch the data for the requested point using the provided index.
    TrailPoint p = points[in_point_storage_index];

    // Pass the relevant data directly to the geometry shader.
    gs_in.position = p.position.xyz;
    gs_in.color = p.color.xyz;
    gs_in.timestamp = p.timestamp;
}
