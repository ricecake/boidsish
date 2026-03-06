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

// Intersect a ray with a single triangle
float intersectTriangle(vec3 origin, vec3 dir, vec3 v0, vec3 v1, vec3 v2) {
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec3 h = cross(dir, e2);
    float a = dot(e1, h);
    if (a > -0.00001 && a < 0.00001) return 1e30;
    float f = 1.0 / a;
    vec3 s = origin - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return 1e30;
    vec3 q = cross(s, e1);
    float v = f * dot(dir, q);
    if (v < 0.0 || u + v > 1.0) return 1e30;
    float t = f * dot(e2, q);
    if (t > 0.00001) return t;
    return 1e30;
}

// Instanced Ray-LBVH nearest intersection
// tlas_root: root of the top-level BVH (instances)
// tlas_nodes_binding: binding point for TLAS nodes
// blas_nodes_binding: binding point for BLAS nodes (triangles)
// instance_matrices_binding: binding point for instance world matrices
// triangle_indices_binding: binding point for triangle indices
// triangle_vertices_binding: binding point for triangle vertices
int lbvh_raycast_instanced(
    vec3 origin, vec3 dir, out float hit_t,
    int tlas_root,
    int tlas_nodes_binding,
    int blas_nodes_binding,
    int instance_matrices_binding,
    int triangle_indices_binding,
    int triangle_vertices_binding
) {
    hit_t = 1e30;
    int hit_instance_idx = -1;
    int hit_triangle_idx = -1;

    if (tlas_root < 0) return -1;

    int stack[32];
    int stack_ptr = 0;
    stack[stack_ptr++] = tlas_root;

    // TLAS Traversal
    while (stack_ptr > 0) {
        int node_idx = stack[--stack_ptr];

        // Use manual binding access since we can't easily switch buffer bindings dynamically in GLSL
        // We assume the caller has set up these specific bindings
        LBVHNode node = u_lbvhNodes[node_idx]; // TLAS nodes assumed to be in binding 20 by default or via u_lbvhNodes

        float t = intersectAABB(origin, dir, node.min_pt_left.xyz, node.max_pt_right.xyz);
        if (t < hit_t) {
            if (node.object_idx != -1) {
                int instance_idx = node.object_idx;

                // Get instance matrix (binding 0 for DecorInstances)
                // mat4 world_mat = ssboInstances[instance_idx]; // This needs another binding
                // We'll need a way to access these. Let's assume standard bindings for now.

                // For now, let's keep the logic abstract and refine it in the implementation of the validation shader
                // because GLSL doesn't support dynamic binding indices for buffers.
            } else {
                stack[stack_ptr++] = int(node.max_pt_right.w);
                stack[stack_ptr++] = int(node.min_pt_left.w);
            }
        }
        if (stack_ptr >= 32) break;
    }
    return hit_instance_idx;
}

#endif
