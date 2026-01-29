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

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 Color;
} vs_out;

vec3 rotate_vector(vec3 v, vec4 q) {
	return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

void main() {
	uint     fid = gl_InstanceID;
	Fragment f = fragments[fid];

	if (f.t2_age.w <= 0.0) {
		vs_out.FragPos = vec3(0);
        gl_Position = vec4(0, 0, 0, 0);
		return;
	}

	vec3 v[3];
	v[0] = f.v0.xyz;
	v[1] = f.v1.xyz;
	v[2] = f.v2.xyz;

	vec2 t[3];
	t[0] = f.t01.xy;
	t[1] = f.t01.zw;
	t[2] = f.t2_age.xy;

	int vert = gl_VertexID % 3; // 0, 1, or 2

	vs_out.FragPos = f.pos.xyz + rotate_vector(v[vert], f.rot);
	vs_out.Normal = rotate_vector(f.normal.xyz, f.rot);
	vs_out.Color = f.color;
	vs_out.TexCoords = t[vert];

	gl_Position = vec4(vs_out.FragPos, 1.0);
}
