#version 430 core

struct Fragment {
	vec4 v0;
	vec4 v1;
	vec4 v2;
	vec4 t01;
	vec4 t2_age;
	vec4 normal;
	vec4 pos;
	vec4 vel;
	vec4 rot;
	vec4 angVel;
	vec4 color;
};

layout(std430, binding = 0) buffer FragmentBuffer {
	Fragment fragments[];
};

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoords;
out vec4 vColor;
out vec3 vBaryThickness;

vec3 rotate_vector(vec3 v, vec4 q) {
	return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

void main() {
	uint     gid = gl_InstanceID;
	Fragment f = fragments[gid];

	if (f.t2_age.w <= 0.0) {
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // Outside clip space
		vColor = vec4(0.0);
		return;
	}

	vec3 p;
	vec2 uv;
	if (gl_VertexID == 0) {
		p = f.v0.xyz;
		uv = f.t01.xy;
	} else if (gl_VertexID == 1) {
		p = f.v1.xyz;
		uv = f.t01.zw;
	} else {
		p = f.v2.xyz;
		uv = f.t2_age.xy;
	}

	// Rotation
	vec3 rotated_p = rotate_vector(p, f.rot);
	vec3 world_pos = f.pos.xyz + rotated_p;

	vWorldPos = world_pos;
	vNormal = rotate_vector(f.normal.xyz, f.rot);
	vTexCoords = uv;
	vColor = f.color;
	vBaryThickness = vec3(f.v0.w, f.v1.w, f.v2.w);

	gl_Position = vec4(world_pos, 1.0);
}
