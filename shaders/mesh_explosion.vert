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

uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 Color;

vec3 rotate_vector(vec3 v, vec4 q) {
	return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

void main() {
	uint     fid = gl_InstanceID;
	Fragment f = fragments[fid];

	if (f.t2_age.w <= 0.0) {
		gl_Position = vec4(0, 0, 0, 0);
		return;
	}

	vec3 v0 = f.v0.xyz;
	vec3 v1 = f.v1.xyz;
	vec3 v2 = f.v2.xyz;
	vec3 n = normalize(f.normal.xyz);

	// Calculate a 4th vertex for 3D substance
	float side1 = length(v1 - v0);
	float side2 = length(v2 - v1);
	float side3 = length(v0 - v2);
	float thickness = (side1 + side2 + side3) * 0.15; // Proportional thickness
	vec3  v3 = -n * thickness;

	vec3 localPos;
	vec3 localNormal;
	vec2 tex;

	int face = gl_VertexID / 3;
	int vert = gl_VertexID % 3;

	vec2 t0 = f.t01.xy;
	vec2 t1 = f.t01.zw;
	vec2 t2 = f.t2_age.xy;
	vec2 t3 = (t0 + t1 + t2) / 3.0;

	if (face == 0) { // Top face
		if (vert == 0) {
			localPos = v0;
			tex = t0;
		} else if (vert == 1) {
			localPos = v1;
			tex = t1;
		} else {
			localPos = v2;
			tex = t2;
		}
		localNormal = n;
	} else if (face == 1) { // Side 1
		if (vert == 0) {
			localPos = v0;
			tex = t0;
		} else if (vert == 1) {
			localPos = v2;
			tex = t2;
		} else {
			localPos = v3;
			tex = t3;
		}
		localNormal = normalize(cross(v2 - v0, v3 - v0));
	} else if (face == 2) { // Side 2
		if (vert == 0) {
			localPos = v1;
			tex = t1;
		} else if (vert == 1) {
			localPos = v0;
			tex = t0;
		} else {
			localPos = v3;
			tex = t3;
		}
		localNormal = normalize(cross(v0 - v1, v3 - v1));
	} else { // Side 3
		if (vert == 0) {
			localPos = v2;
			tex = t2;
		} else if (vert == 1) {
			localPos = v1;
			tex = t1;
		} else {
			localPos = v3;
			tex = t3;
		}
		localNormal = normalize(cross(v1 - v2, v3 - v2));
	}

	vec3 worldPos = f.pos.xyz + rotate_vector(localPos, f.rot);
	Normal = rotate_vector(localNormal, f.rot);
	FragPos = worldPos;
	Color = f.color;
	TexCoords = tex;

	gl_Position = u_projection * u_view * vec4(worldPos, 1.0);
}
