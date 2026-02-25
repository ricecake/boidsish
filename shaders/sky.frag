#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

#include "atmosphere/common.glsl"
#include "helpers/lighting.glsl"

uniform mat4 invProjection;
uniform mat4 invView;

uniform sampler2D u_transmittanceLUT;
uniform sampler2D u_skyViewLUT;

uniform vec3 u_sunRadiance; // Added for consistency with scattering

vec3 getTransmittance(float r, float mu) {
	vec2 uv = transmittanceToUV(r, mu);
	return texture(u_transmittanceLUT, uv).rgb;
}

vec3 sampleSkyView(vec3 rd) {
	float elevation = asin(clamp(rd.y, -1.0, 1.0));
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;

	// Non-linear mapping for better horizon detail
	float v = (elevation < 0.0) ? (0.5 - 0.5 * sqrt(-elevation / (PI * 0.5)))
								: (0.5 + 0.5 * sqrt(elevation / (PI * 0.5)));
	vec2  uv = vec2(azimuth / (2.0 * PI), v);
	return texture(u_skyViewLUT, uv).rgb;
}

// Fixed snoise with correct permutation mapping
vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(vec4 r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}

vec2 fade(vec2 t) {
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float snoise(vec3 v) {
	const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
	const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
	vec3       i = floor(v + dot(v, C.yyy));
	vec3       x0 = v - i + dot(i, C.xxx);
	vec3       g = step(x0.yzx, x0.xyz);
	vec3       l = 1.0 - g;
	vec3       i1 = min(g.xyz, l.zxy);
	vec3       i2 = max(g.xyz, l.zxy);
	vec3       x1 = x0 - i1 + C.xxx;
	vec3       x2 = x0 - i2 + C.yyy;
	vec3       x3 = x0 - D.yyy;
	i = mod289(i);
	vec4 p = permute(
		permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x +
		vec4(0.0, i1.x, i2.x, 1.0)
	);
	float n_ = 0.142857142857;
	vec3  ns = n_ * D.wyz - D.xzx;
	vec4  j = p - 49.0 * floor(p * ns.z * ns.z);
	vec4  x_ = floor(j * ns.z);
	vec4  y_ = floor(j - 7.0 * x_);
	vec4  x = x_ * ns.x + ns.yyyy;
	vec4  y = y_ * ns.x + ns.yyyy;
	vec4  h = 1.0 - abs(x) - abs(y);
	vec4  b0 = vec4(x.xy, y.xy);
	vec4  b1 = vec4(x.zw, y.zw);
	vec4  s0 = floor(b0) * 2.0 + 1.0;
	vec4  s1 = floor(b1) * 2.0 + 1.0;
	vec4  sh = -step(h, vec4(0.0));
	vec4  a0 = b0.xzyw + s0.xzyw * sh.xxyy;
	vec4  a1 = b1.xzyw + s1.xzyw * sh.zzww; // Fixed: s1.xzyw instead of s1.zzww
	vec3  p0 = vec3(a0.xy, h.x);
	vec3  p1 = vec3(a0.zw, h.y);
	vec3  p2 = vec3(a1.xy, h.z);
	vec3  p3 = vec3(a1.zw, h.w);
	vec4  norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;
	vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
	m = m * m;
	return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

vec3 hash33(vec3 p) {
	p = fract(p * vec3(443.897, 441.423, 437.195));
	p += dot(p, p.yxz + 19.19);
	return fract((p.xxy + p.yxx) * p.zyx);
}

float fbm(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		v += a * snoise(p);
		p *= 2.0;
		a *= 0.5;
	}
	return v;
}

float starLayer(vec3 dir) {
	float scale = 100.0;
	vec3  id = floor(dir * scale);
	vec3  local_uv = fract(dir * scale);
	vec3  star_pos = hash33(id);
	float brightness = abs(sin(time / 2 + star_pos.x * 100));
	vec3  center = vec3(0.5) + (star_pos - 0.5) * 0.8;
	float dist = length(local_uv - center);
	float radius = 0.05 * brightness;
	return smoothstep(radius, radius * 0.5, dist);
}

void main() {
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

	vec3 sunDir = vec3(0, 1, 0);
	vec3 sunColor = vec3(1);
	if (num_lights > 0) {
		sunDir = normalize(-lights[0].direction);
		sunColor = lights[0].color;
	}

	// 1. Atmospheric Scattering
	vec3 skyRadiance = sampleSkyView(world_ray);

	// 2. Sun Disc with Realistic Distortion (flattening near horizon)
	float cosTheta = dot(world_ray, sunDir);
	float sunAngularRadius = 0.02; // approx 1.0 degrees

	// Construct local sun frame
	vec3 sunUp = vec3(0, 1, 0);
	vec3 sunRight = normalize(cross(sunUp, sunDir));
	sunUp = cross(sunDir, sunRight);

	// Project ray into sun frame
	float rayLocalX = dot(world_ray, sunRight);
	float rayLocalY = dot(world_ray, sunUp);
	float rayLocalZ = dot(world_ray, sunDir);

	// Distort vertical axis near horizon
	float atmosphericRefraction = 1.0 + 0.6 * exp(-max(0.0, sunDir.y * 10.0));
	float flattenedY = rayLocalY * atmosphericRefraction;

	// Effective angle to sun center
	float distSq = rayLocalX * rayLocalX + flattenedY * flattenedY;
	float sunMask = smoothstep(
		sunAngularRadius * sunAngularRadius,
		(sunAngularRadius - 0.001) * (sunAngularRadius - 0.001),
		distSq
	);
	// Ensure we are in front of the sun
	sunMask *= step(0.99, rayLocalZ);

	float r = kEarthRadius + viewPos.y / 1000.0;
	vec3  sunTransmittance = getTransmittance(r, sunDir.y);
	// Use u_sunRadiance if available (via AtmosphereManager) or fallback to simple sunColor
	vec3 radiance = length(u_sunRadiance) > 0.0 ? u_sunRadiance : (sunColor * 20.0);
	vec3 sunDisc = radiance * sunMask * sunTransmittance;

	// 3. Stars and Nebula
	vec3 stars = starLayer(world_ray) * vec3(1.0, 0.9, 0.8);

	vec3  p = world_ray * 4.0;
	vec3  warp_offset = vec3(fbm(p + time * 0.05));
	float nebula_noise = fbm(p + warp_offset * 0.5);
	vec3  nebula = mix(vec3(0.0, 0.1, 0.4), vec3(0.8, 0.2, 0.7), nebula_noise) * 0.4;

	vec3 skyTransmittance = getTransmittance(r, world_ray.y);
	vec3 spaceBackground = (stars + nebula) * skyTransmittance;

	vec3 finalColor = skyRadiance + sunDisc + spaceBackground;

	FragColor = vec4(finalColor, 1.0);
}
