#ifndef SIMPLEX_EXACT_GLSL
#define SIMPLEX_EXACT_GLSL

layout(std140, binding = 10) uniform SimplexData {
	ivec4 u_perm[128]; // 512 entries packed
};

int getPerm(int idx) {
	return u_perm[(idx >> 2) & 127][idx & 3];
}

// 2D Simplex gradient directions
const vec2 grad2lut[8] = vec2[](
	vec2(-1.0, -1.0),
	vec2(1.0, 0.0),
	vec2(-1.0, 0.0),
	vec2(1.0, 1.0),
	vec2(-1.0, 1.0),
	vec2(0.0, -1.0),
	vec2(0.0, 1.0),
	vec2(1.0, -1.0)
);

// 3D Simplex gradient directions
const vec3 grad3lut[16] = vec3[](
	vec3(1.0, 0.0, 1.0),
	vec3(0.0, 1.0, 1.0),
	vec3(-1.0, 0.0, 1.0),
	vec3(0.0, -1.0, 1.0),
	vec3(1.0, 0.0, -1.0),
	vec3(0.0, 1.0, -1.0),
	vec3(-1.0, 0.0, -1.0),
	vec3(0.0, -1.0, -1.0),
	vec3(1.0, -1.0, 0.0),
	vec3(1.0, 1.0, 0.0),
	vec3(-1.0, 1.0, 0.0),
	vec3(-1.0, -1.0, 0.0),
	vec3(1.0, 0.0, 1.0),
	vec3(-1.0, 0.0, 1.0),
	vec3(0.0, 1.0, -1.0),
	vec3(0.0, -1.0, -1.0)
);

float grad2(int hash, vec2 v) {
	int   h = hash & 7;
	float u = h < 4 ? v.x : v.y;
	float v_ = h < 4 ? v.y : v.x;
	return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -2.0 * v_ : 2.0 * v_);
}

float grad3(int hash, vec3 v) {
	int   h = hash & 15;
	float u = h < 8 ? v.x : v.y;
	float v_ = h < 4 ? v.y : (h == 12 || h == 14 ? v.x : v.z);
	return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -v_ : v_);
}

const float F2 = 0.366025403; // 0.5 * (sqrt(3.0) - 1.0)
const float G2 = 0.211324865; // (3.0 - sqrt(3.0)) / 6.0
const float F3 = 0.333333333; // 1/3
const float G3 = 0.166666667; // 1/6

float simplexNoise2D(vec2 v) {
	float n0, n1, n2;

	float s = (v.x + v.y) * F2;
	int   i = int(floor(v.x + s));
	int   j = int(floor(v.y + s));

	float t = float(i + j) * G2;
	float X0 = float(i) - t;
	float Y0 = float(j) - t;
	vec2  v0 = v - vec2(X0, Y0);

	int i1, j1;
	if (v0.x > v0.y) {
		i1 = 1;
		j1 = 0;
	} else {
		i1 = 0;
		j1 = 1;
	}

	vec2 v1 = v0 - vec2(i1, j1) + G2;
	vec2 v2 = v0 - vec2(1.0, 1.0) + 2.0 * G2;

	int ii = i & 0xff;
	int jj = j & 0xff;

	float t0 = 0.5 - dot(v0, v0);
	if (t0 < 0.0)
		n0 = 0.0;
	else {
		t0 *= t0;
		n0 = t0 * t0 * grad2(getPerm(ii + getPerm(jj)), v0);
	}

	float t1 = 0.5 - dot(v1, v1);
	if (t1 < 0.0)
		n1 = 0.0;
	else {
		t1 *= t1;
		n1 = t1 * t1 * grad2(getPerm(ii + i1 + getPerm(jj + j1)), v1);
	}

	float t2 = 0.5 - dot(v2, v2);
	if (t2 < 0.0)
		n2 = 0.0;
	else {
		t2 *= t2;
		n2 = t2 * t2 * grad2(getPerm(ii + 1 + getPerm(jj + 1)), v2);
	}

	return 40.0 * (n0 + n1 + n2);
}

// Analytical derivative version of 2D Simplex Noise
vec3 simplexDNoise2D(vec2 v) {
	float s = (v.x + v.y) * F2;
	int   i = int(floor(v.x + s));
	int   j = int(floor(v.y + s));

	float t = float(i + j) * G2;
	float X0 = float(i) - t;
	float Y0 = float(j) - t;
	vec2  v0 = v - vec2(X0, Y0);

	int i1, j1;
	if (v0.x > v0.y) {
		i1 = 1;
		j1 = 0;
	} else {
		i1 = 0;
		j1 = 1;
	}

	vec2 v1 = v0 - vec2(float(i1), float(j1)) + G2;
	vec2 v2 = v0 - vec2(1.0, 1.0) + 2.0 * G2;

	int ii = i & 0xff;
	int jj = j & 0xff;

	vec2 g0, g1, g2;
	int  h0 = getPerm(ii + getPerm(jj)) & 7;
	int  h1 = getPerm(ii + i1 + getPerm(jj + j1)) & 7;
	int  h2 = getPerm(ii + 1 + getPerm(jj + 1)) & 7;
	g0 = grad2lut[h0];
	g1 = grad2lut[h1];
	g2 = grad2lut[h2];

	float t0 = 0.5 - dot(v0, v0);
	float t20, t40, n0;
	if (t0 < 0.0) {
		t40 = t20 = t0 = n0 = 0.0;
		g0 = vec2(0.0);
	} else {
		t20 = t0 * t0;
		t40 = t20 * t20;
		n0 = t40 * dot(g0, v0);
	}

	float t1 = 0.5 - dot(v1, v1);
	float t21, t41, n1;
	if (t1 < 0.0) {
		t41 = t21 = t1 = n1 = 0.0;
		g1 = vec2(0.0);
	} else {
		t21 = t1 * t1;
		t41 = t21 * t21;
		n1 = t41 * dot(g1, v1);
	}

	float t2 = 0.5 - dot(v2, v2);
	float t22, t42, n2;
	if (t2 < 0.0) {
		t42 = t22 = t2 = n2 = 0.0;
		g2 = vec2(0.0);
	} else {
		t22 = t2 * t2;
		t42 = t22 * t22;
		n2 = t42 * dot(g2, v2);
	}

	float temp0 = t20 * t0 * dot(g0, v0);
	vec2  d0 = temp0 * v0;
	float temp1 = t21 * t1 * dot(g1, v1);
	vec2  d1 = temp1 * v1;
	float temp2 = t22 * t2 * dot(g2, v2);
	vec2  d2 = temp2 * v2;

	vec2 dnoise = -8.0 * (d0 + d1 + d2) + (t40 * g0 + t41 * g1 + t42 * g2);
	return vec3(40.0 * (n0 + n1 + n2), 40.0 * dnoise);
}

// 2D Curl Noise using Simplex analytical derivatives
vec2 simplexCurlNoise2D(vec2 v) {
	vec3 d = simplexDNoise2D(v);
	return vec2(d.z, -d.y);
}

#endif
