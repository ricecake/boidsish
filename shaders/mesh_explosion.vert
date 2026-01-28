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

	vec3 localPos;
	if (gl_VertexID == 0) {
		localPos = f.v0.xyz;
		TexCoords = f.t01.xy;
	} else if (gl_VertexID == 1) {
		localPos = f.v1.xyz;
		TexCoords = f.t01.zw;
	} else {
		localPos = f.v2.xyz;
		TexCoords = f.t2_age.xy;
	}

	vec3 worldPos = f.pos.xyz + rotate_vector(localPos, f.rot);
	Normal = rotate_vector(f.normal.xyz, f.rot);
	FragPos = worldPos;
	Color = f.color;

	gl_Position = u_projection * u_view * vec4(worldPos, 1.0);
}
