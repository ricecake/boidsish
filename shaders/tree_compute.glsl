#version 430 core

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// === Structs (must match C++ layout) ===
struct AttractionPoint {
    vec4 position;
    int isActive;
    vec3 padding;
};

struct Branch {
    vec4 position;
    vec4 parent_position;
    int parent_index;
    float thickness;
    vec2 padding;
};

// === Buffers ===
layout(std430, binding = 0) buffer AttractionPoints {
    AttractionPoint attraction_points[];
};

layout(std430, binding = 1) buffer TreeBranches {
    Branch tree_branches[];
};

layout(binding = 2, offset = 0) uniform atomic_uint branch_counter;

layout(std430, binding = 3) buffer BranchGrownLock {
    int branch_grown[]; // Use as a lock: 0 = not grown, 1 = grown this frame
};

// === Uniforms ===
uniform int num_attraction_points;
uniform float kill_radius_sq;
uniform float attraction_radius_sq;
uniform float branch_length;
uniform int max_branches;

// === Shared Memory ===
// Note: The following variables were previously marked as 'shared', which was incorrect
// as it created a race condition. They are now thread-local variables.
int closest_branch_index;
float min_dist_sq;
vec3 accumulated_direction;
int influence_count;

void main() {
    uint global_id = gl_GlobalInvocationID.x;

    // --- Part 1: Deactivate nearby attraction points ---
    // Each thread checks one attraction point
    if (global_id < num_attraction_points && attraction_points[global_id].isActive > 0) {
        uint branch_count = atomicCounter(branch_counter);
        for (int i = 0; i < branch_count; ++i) {
            float dist_sq = dot(
                attraction_points[global_id].position.xyz - tree_branches[i].position.xyz,
                attraction_points[global_id].position.xyz - tree_branches[i].position.xyz
            );
            if (dist_sq < kill_radius_sq) {
                attraction_points[global_id].isActive = 0;
                break; // Move to the next attraction point
            }
        }
    }

    // Synchronize to ensure all deactivations are visible before proceeding
    barrier();
    memoryBarrier();

    // --- Part 2: Find closest branch for each attraction point ---
    // Each thread still works on one attraction point
    if (global_id < num_attraction_points && attraction_points[global_id].isActive > 0) {
        min_dist_sq = attraction_radius_sq;
        closest_branch_index = -1;
        uint branch_count = atomicCounter(branch_counter);

        for (int i = 0; i < branch_count; ++i) {
            float dist_sq = dot(
                attraction_points[global_id].position.xyz - tree_branches[i].position.xyz,
                attraction_points[global_id].position.xyz - tree_branches[i].position.xyz
            );
            if (dist_sq < min_dist_sq) {
                min_dist_sq = dist_sq;
                closest_branch_index = i;
            }
        }

        // --- Part 3: Accumulate influence and grow new branches ---
        if (closest_branch_index != -1) {
            // This is the key part: atomically "lock" the branch to grow.
            // Only one thread (attraction point) can succeed here per frame.
            if (atomicCompSwap(branch_grown[closest_branch_index], 0, 1) == 0) {

                // This thread won the race and is responsible for growing the branch.
                // It now accumulates influence from all nearby attraction points.
                accumulated_direction = vec3(0.0);
                influence_count = 0;

                for (int j = 0; j < num_attraction_points; ++j) {
                    if (attraction_points[j].isActive > 0) {
                         float dist_sq_to_branch = dot(
                            attraction_points[j].position.xyz - tree_branches[closest_branch_index].position.xyz,
                            attraction_points[j].position.xyz - tree_branches[closest_branch_index].position.xyz
                        );

                        // If the point is within the influence radius, add its direction.
                        if(dist_sq_to_branch < attraction_radius_sq) {
                            accumulated_direction += normalize(attraction_points[j].position.xyz - tree_branches[closest_branch_index].position.xyz);
                            influence_count++;
                        }
                    }
                }

                // Average the direction if any points had influence.
                if (influence_count > 0) {
                    vec3 avg_dir = normalize(accumulated_direction);

                    // Get a new index for our branch.
                    uint new_branch_index = atomicCounterIncrement(branch_counter);

                if (new_branch_index < max_branches) {
                    Branch parent_branch = tree_branches[closest_branch_index];

                    Branch new_branch;
                    new_branch.position = parent_branch.position + vec4(avg_dir * branch_length, 0.0);
                    new_branch.parent_position = parent_branch.position;
                    new_branch.parent_index = closest_branch_index;
                    new_branch.thickness = parent_branch.thickness * 0.95; // Get slightly thinner

                    tree_branches[new_branch_index] = new_branch;
                }
            }
        }
    }
}
