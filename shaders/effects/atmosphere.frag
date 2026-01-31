#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;

uniform vec3 cameraPos;
uniform mat4 invView;
uniform mat4 invProjection;

uniform float hazeDensity;
uniform float hazeHeight;
uniform vec3  hazeColor;
uniform float cloudDensity;
uniform float cloudAltitude;
uniform float cloudThickness;
uniform vec3  cloudColorUniform;
// uniform float time;

#include "../helpers/lighting.glsl"
#include "../helpers/noise.glsl"

// vec3 mod289(vec3 x) {
// 	return x - floor(x * (1.0 / 289.0)) * 289.0;
// }

vec4 mod289(vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(vec4 r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}

// vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec2 fade(vec2 t) {
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float snoise(vec3 v) {
	const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
	const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

	// First corner
	vec3 i = floor(v + dot(v, C.yyy));
	vec3 x0 = v - i + dot(i, C.xxx);

	// Other corners
	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min(g.xyz, l.zxy);
	vec3 i2 = max(g.xyz, l.zxy);

	vec3 x1 = x0 - i1 + C.xxx;
	vec3 x2 = x0 - i2 + C.yyy;
	vec3 x3 = x0 - D.yyy;

	// Permutations
	i = mod289(i);
	vec4 p = permute(
		permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x +
		vec4(0.0, i1.x, i2.x, 1.0)
	);

	float n_ = 0.142857142857;
	vec3  ns = n_ * D.wyz - D.xzx;

	vec4 j = p - 49.0 * floor(p * ns.z * ns.z);

	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_);

	vec4 x = x_ * ns.x + ns.yyyy;
	vec4 y = y_ * ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);

	vec4 b0 = vec4(x.xy, y.xy);
	vec4 b1 = vec4(x.zw, y.zw);

	vec4 s0 = floor(b0) * 2.0 + 1.0;
	vec4 s1 = floor(b1) * 2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));

	vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
	vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

	vec3 p0 = vec3(a0.xy, h.x);
	vec3 p1 = vec3(a0.zw, h.y);
	vec3 p2 = vec3(a1.xy, h.z);
	vec3 p3 = vec3(a1.zw, h.w);

	// Normalise gradients
	vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

	// Mix final noise value
	vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
	m = m * m;
	return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}


float fbm(vec3 p) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; i++) {
		value += amplitude * snoise(p);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}


float getHeightFog(vec3 start, vec3 end, float density, float heightFalloff, float depth) {
	// end = min(end, (end-start)*depth);
	float dist = length(end - start);
	// dist = min(dist, depth);
	vec3  dir = (end - start) / dist;

	// vec3  p = dir * 4.0;
	// vec3  warp_offset = vec3(fbm(p + time * 0.015));
	// float nebula_noise = fbm(p + warp_offset * 0.5);
	vec3  p = dir * 4.0;
	vec3  warp_offset = vec3(fbm(p + time * 0.015));
	float nebula_noise = fbm(p + warp_offset * 0.5);

	// density *= nebula_noise * depth;
	float fog;
	if (abs(dir.y) < 0.0001) {
		fog = density * exp(-heightFalloff * start.y) * depth * nebula_noise;
	} else {
		// fog = (density / (heightFalloff * dir.y)) * (exp(-heightFalloff * start.y) - exp(-heightFalloff * end.y));
		fog = (density / (heightFalloff * dir.y)) * (exp(-heightFalloff * start.y) - exp(-heightFalloff * end.y)) * nebula_noise;
	}
	return 1.0 - exp(-max(0.0, fog));
}

float fbm(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		v += a * snoise(p);
		p *= 2.0;
		a *= 0.5;
	}
	return v;
}

// Simple Mie scattering approximation
float scatter(vec3 lightDir, vec3 viewDir, float g) {
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * 3.14159 * pow(1.0 + g2 - 2.0 * g * dot(lightDir, viewDir), 1.5));
}

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;

	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3 worldPos = (invView * viewSpacePosition).xyz;

	vec3  rayDir = normalize(worldPos - cameraPos);
	float dist = length(worldPos - cameraPos);

	if (depth == 1.0) {
		dist = 1000.0; // Assume sky is far
		worldPos = cameraPos + rayDir * dist;
	}

	// 1. Height Fog (Haze)
	float fogFactor = getHeightFog(cameraPos, worldPos, hazeDensity, 1.0 / (hazeHeight + 0.001), depth);
	vec3  currentHazeColor = hazeColor;

	// Add light scattering to fog
	vec3 scattering = vec3(0.0);
	for (int i = 0; i < num_lights; i++) {
		vec3  lightDir = normalize(lights[i].position - cameraPos);
		float s = scatter(lightDir, rayDir, 0.7);
		scattering += lights[i].color * s * lights[i].intensity * 0.05;
	}
	currentHazeColor += scattering;

	// 2. Cloud Layer
	float cloudFactor = 0.0;
	vec3  cloudColor = vec3(0.0);

	// Intersect with cloud layer (volume approximation)
	float t_start = (cloudAltitude - cameraPos.y) / (rayDir.y + 0.000001);
	float t_end = (cloudAltitude + cloudThickness - cameraPos.y) / (rayDir.y + 0.000001);

	if (t_start > t_end) {
		float temp = t_start;
		t_start = t_end;
		t_end = temp;
	}

	t_start = max(t_start, 0.0);
	t_end = min(t_end, dist);

	if (t_start < t_end) {
		float cloudAcc = 0.0;
		int   samples = 6;
		float jitter = fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453);

		for (int i = 0; i < samples; i++) {
			float t = mix(t_start, t_end, (float(i) + jitter) / float(samples));
			vec3  p = cameraPos + rayDir * t;
			float h = (p.y - cloudAltitude) / max(cloudThickness, 0.001);
			float tapering = smoothstep(0.0, 0.2, h) * smoothstep(1.0, 0.5, h);

			float noise = fbm(p.xz * 0.015 + jitter * time * 0.0001 + p.y * 0.02);
			// float d = smoothstep(0.2, 0.6, noise * (i + 1)) * cloudDensity;
			float d = smoothstep(0.2, 0.6, noise * (i + (1 - noise))) * cloudDensity * tapering;

			cloudAcc += d;
		}
		cloudFactor = 1.0 - exp(-cloudAcc * (t_end - t_start) * 0.05 / float(samples));

		// Cloud lighting at the center of the cloud intersection
		vec3 intersect = cameraPos + rayDir * mix(t_start, t_end, 0.5);
		vec3 cloudScattering = vec3(0.0);
		for (int i = 0; i < num_lights; i++) {
			vec3  L = normalize(lights[i].position - intersect);
			float d = max(0.0, dot(vec3(0, 1, 0), L)); // Simple top-lighting
			float silver = pow(max(0.0, dot(rayDir, L)), 4.0) * 0.5;

			cloudScattering += lights[i].color * (d * 0.5 + 0.5 + silver) * lights[i].intensity;
		}

		cloudColor = cloudColorUniform * (ambient_light + cloudScattering * 0.5);
	}

	// Combine everything
	vec3 result = mix(sceneColor, cloudColor, cloudFactor);
	result = mix(result, currentHazeColor, fogFactor);

	FragColor = vec4(result, 1.0);
}
