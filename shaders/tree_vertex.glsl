#version 430 core

struct Branch {
    vec4 position;
    vec4 parent_position;
    int parent_index;
    float thickness;
};

layout(std430, binding = 1) buffer TreeBranches {
    Branch tree_branches[];
};

void main() {
    // We don't need to do anything here, since the geometry shader will generate the vertices.
    // However, we need a valid entry point.
}
