#ifndef LBVH_GLSL
#define LBVH_GLSL

// LBVHNode structure with robust std430 alignment
// min_pt.w = left child index
// max_pt.w = right child index
struct LBVHNode {
    vec4 min_pt_left;
    vec4 max_pt_right;
    int parent;
    int object_idx;
    int _pad[2];
};

layout(std430, binding = 20) readonly buffer LBVHNodes {
    LBVHNode u_lbvhNodes[];
};

// Intersection of a ray with an AABB
float intersectAABB(vec3 ray_origin, vec3 ray_dir, vec3 aabb_min, vec3 aabb_max) {
    if (any(greaterThan(aabb_min, aabb_max))) return 1e30;
    vec3 t_min = (aabb_min - ray_origin) / ray_dir;
    vec3 t_max = (aabb_max - ray_origin) / ray_dir;
    vec3 t1 = min(t_min, t_max);
    vec3 t2 = max(t_min, t_max);
    float t_near = max(max(t1.x, t1.y), t1.z);
    float t_far = min(min(t2.x, t2.y), t2.z);

    if (t_near <= t_far && t_far > 0.0) {
        return t_near > 0.0 ? t_near : 0.0;
    }
    return 1e30;
}

// Ray-LBVH occlusion test
bool lbvh_occluded(vec3 origin, vec3 dir, float max_dist, int root_idx) {
    if (root_idx < 0) return false;

    int stack[32];
    int stack_ptr = 0;
    stack[stack_ptr++] = root_idx;

    while (stack_ptr > 0) {
        int node_idx = stack[--stack_ptr];
        LBVHNode node = u_lbvhNodes[node_idx];

        float t = intersectAABB(origin, dir, node.min_pt_left.xyz, node.max_pt_right.xyz);
        if (t < max_dist) {
            if (node.object_idx != -1) {
                return true;
            } else {
                stack[stack_ptr++] = int(node.max_pt_right.w); // right
                stack[stack_ptr++] = int(node.min_pt_left.w);  // left
            }
        }
        if (stack_ptr >= 32) break;
    }
    return false;
}

// Ray-LBVH nearest intersection
int lbvh_raycast(vec3 origin, vec3 dir, out float hit_t, int root_idx) {
    hit_t = 1e30;
    int hit_idx = -1;
    if (root_idx < 0) return -1;

    int stack[32];
    int stack_ptr = 0;
    stack[stack_ptr++] = root_idx;

    while (stack_ptr > 0) {
        int node_idx = stack[--stack_ptr];
        LBVHNode node = u_lbvhNodes[node_idx];

        float t = intersectAABB(origin, dir, node.min_pt_left.xyz, node.max_pt_right.xyz);
        if (t < hit_t) {
            if (node.object_idx != -1) {
                hit_t = t;
                hit_idx = node.object_idx;
            } else {
                stack[stack_ptr++] = int(node.max_pt_right.w); // right
                stack[stack_ptr++] = int(node.min_pt_left.w);  // left
            }
        }
        if (stack_ptr >= 32) break;
    }
    return hit_idx;
}

// Frustum culling helper
bool lbvh_frustum_test(vec4 frustum[6], int root_idx, out int visible_indices[128], out int visible_count) {
    visible_count = 0;
    if (root_idx < 0) return false;

    int stack[32];
    int stack_ptr = 0;
    stack[stack_ptr++] = root_idx;

    while (stack_ptr > 0 && visible_count < 128) {
        int node_idx = stack[--stack_ptr];
        LBVHNode node = u_lbvhNodes[node_idx];

        vec3 center = (node.min_pt_left.xyz + node.max_pt_right.xyz) * 0.5;
        float radius = length(node.max_pt_right.xyz - center);

        bool in_frustum = true;
        for (int i = 0; i < 6; i++) {
            if (dot(frustum[i].xyz, center) + frustum[i].w < -radius) {
                in_frustum = false;
                break;
            }
        }

        if (in_frustum) {
            if (node.object_idx != -1) {
                visible_indices[visible_count++] = node.object_idx;
            } else {
                stack[stack_ptr++] = int(node.max_pt_right.w);
                stack[stack_ptr++] = int(node.min_pt_left.w);
            }
        }
        if (stack_ptr >= 32) break;
    }
    return visible_count > 0;
}

#endif
