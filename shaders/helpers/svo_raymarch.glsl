#ifndef SVO_RAYMARCH_GLSL
#define SVO_RAYMARCH_GLSL

struct SvoNode {
	uint child_mask;
	uint leaf_mask;
	uint child_base_idx;
	uint voxel_base_idx;
};

struct SvoVoxel {
	vec4 color;
};

layout(std430, binding = [[SVO_NODES_BINDING]]) readonly buffer SvoNodesBuffer {
	SvoNode u_svoNodes[];
};

layout(std430, binding = [[SVO_VOXELS_BINDING]]) readonly buffer SvoVoxelsBuffer {
	SvoVoxel u_svoVoxels[];
};

struct SvoHit {
	bool hit;
	vec3 position;
	vec4 color;
	vec3 normal;
	float t;
};

/**
 * @brief Performs SVO raymarching with hierarchical descent and space skipping.
 * @param ray_origin Ray origin in world space / voxel grid space.
 * @param ray_direction Ray direction (must be normalized).
 * @param t_min Minimum ray distance.
 * @param t_max Maximum ray distance.
 * @param max_depth The maximum depth of the SVO tree.
 * @return SvoHit structure indicating hit status, position, normal, color, and ray parameter t.
 */
SvoHit raymarchSvo(vec3 ray_origin, vec3 ray_direction, float t_min, float t_max, uint max_depth) {
	SvoHit result;
	result.hit = false;
	result.t = t_max;
	result.position = vec3(0.0);
	result.color = vec4(0.0);
	result.normal = vec3(0.0);

	vec3 dir = normalize(ray_direction);
	vec3 inv_dir = 1.0 / (dir + vec3(1e-6)); // Avoid division by zero

	float t = t_min;
	vec3 bounds_min = vec3(0.0);
	vec3 bounds_max = vec3(float(1 << max_depth));

	// Intersect ray with SVO root bounds
	vec3 t0 = (bounds_min - ray_origin) * inv_dir;
	vec3 t1 = (bounds_max - ray_origin) * inv_dir;
	vec3 t_near = min(t0, t1);
	vec3 t_far = max(t0, t1);
	float t_start = max(max(t_near.x, t_near.y), t_near.z);
	float t_end = min(min(t_far.x, t_far.y), t_far.z);

	if (t_start > t_end || t_end < 0.0) {
		return result; // Missed SVO root bounds
	}

	t = max(t, t_start);

	// Raymarch loop
	int max_steps = 128;
	for (int step = 0; step < max_steps; ++step) {
		if (t >= t_end || t >= result.t) {
			break;
		}

		vec3 p = ray_origin + t * dir;
		p = clamp(p, bounds_min, bounds_max - vec3(1e-4));

		// Descend the octree
		uint node_idx = 0;
		vec3 node_min = bounds_min;
		vec3 node_max = bounds_max;
		uint current_depth = 0;

		bool found_leaf = false;
		bool found_empty = false;

		while (current_depth < max_depth) {
			vec3 node_center = (node_min + node_max) * 0.5;

			// Determine octant
			uint octant = 0;
			if (p.x >= node_center.x) { octant |= 1u; node_min.x = node_center.x; } else { node_max.x = node_center.x; }
			if (p.y >= node_center.y) { octant |= 2u; node_min.y = node_center.y; } else { node_max.y = node_center.y; }
			if (p.z >= node_center.z) { octant |= 4u; node_min.z = node_center.z; } else { node_max.z = node_center.z; }

			SvoNode node = u_svoNodes[node_idx];

			// Check if octant exists
			if ((node.child_mask & (1u << octant)) == 0u) {
				found_empty = true;
				break;
			}

			// Check if octant is a leaf
			if ((node.leaf_mask & (1u << octant)) != 0u) {
				uint voxel_idx = node.voxel_base_idx + bitCount(node.leaf_mask & ((1u << octant) - 1u));
				result.hit = true;
				result.t = t;
				result.position = p;
				result.color = u_svoVoxels[voxel_idx].color;

				// Calculate hit normal based on entry boundary
				vec3 diff_min = abs(p - node_min);
				vec3 diff_max = abs(p - node_max);
				float min_diff = 1e30;
				vec3 norm = vec3(0.0);
				if (diff_min.x < min_diff) { min_diff = diff_min.x; norm = vec3(-1.0, 0.0, 0.0); }
				if (diff_max.x < min_diff) { min_diff = diff_max.x; norm = vec3(1.0, 0.0, 0.0); }
				if (diff_min.y < min_diff) { min_diff = diff_min.y; norm = vec3(0.0, -1.0, 0.0); }
				if (diff_max.y < min_diff) { min_diff = diff_max.y; norm = vec3(0.0, 1.0, 0.0); }
				if (diff_min.z < min_diff) { min_diff = diff_min.z; norm = vec3(0.0, 0.0, -1.0); }
				if (diff_max.z < min_diff) { min_diff = diff_max.z; norm = vec3(0.0, 0.0, 1.0); }
				result.normal = norm;

				found_leaf = true;
				break;
			}

			// Descend to internal child node
			uint child_offset = bitCount((node.child_mask & ~node.leaf_mask) & ((1u << octant) - 1u));
			node_idx = node.child_base_idx + child_offset;
			current_depth++;
		}

		if (found_leaf) {
			break;
		}

		// Space skipping: step to the exit boundary of the current cell
		vec3 t_bot = (node_min - ray_origin) * inv_dir;
		vec3 t_top = (node_max - ray_origin) * inv_dir;
		vec3 t_exit_axes = max(t_bot, t_top);
		float t_exit = min(min(t_exit_axes.x, t_exit_axes.y), t_exit_axes.z);

		// Advance past exit boundary
		t = max(t + 1e-4, t_exit + 1e-4);
	}

	return result;
}

#endif // SVO_RAYMARCH_GLSL
