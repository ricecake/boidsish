#version 430 core
#extension GL_GOOGLE_include_directive : enable

out vec4 FragColor;
in vec2  TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform vec2      screenSize;
uniform vec3      cameraPos;
uniform mat4      invView;
uniform mat4      invProjection;
uniform float     time;

struct SdfSource {
	vec4 position_radius;  // xyz: pos, w: radius
	vec4 color_smoothness; // rgb: color, a: smoothness
	vec4 params;           // x: charge, y: type, z: noise_intensity, w: noise_scale
};

layout(std140) uniform SdfVolumes {
	int       numSources;
	SdfSource sources[128];
};

#include "lygia/sdf/sphereSDF.glsl"
#include "../helpers/fast_noise.glsl"

#define TYPE_SPHERE 0
#define TYPE_EXPLOSION 1
#define TYPE_VOLUMETRIC 2

// --- Helper Functions ---

const float nudge = 4.0;
const float normalizer = 1.0 / sqrt(1.0 + nudge * nudge);

float SpiralNoiseC(vec3 p) {
	float n = -mod(time * 1.2, 2.0);
	float iter = 2.0;
	for (int i = 0; i < 8; i++) {
		n += -abs(sin(p.y * iter) + cos(p.x * iter)) / iter;
		p.xy += vec2(p.y, -p.x) * nudge;
		p.xy *= normalizer;
		p.xz += vec2(p.z, -p.x) * nudge;
		p.xz *= normalizer;
		iter *= 1.733733;
	}
	return n;
}

float VolumetricExplosion(vec3 p, vec3 center, float radius, float noise_intensity, float noise_scale) {
	float d = sphereSDF(p - center, radius);
	d += fastFbm3d(p * 50.0 * noise_scale) * 0.1 * noise_intensity;
	d += SpiralNoiseC((p - center) * 0.4 * noise_scale + 333.0) * noise_intensity;
	return d;
}

vec3 computeVolumetricColor(float density, float dist_to_center) {
	vec3 result = mix(vec3(1.0, 0.9, 0.8), vec3(0.4, 0.15, 0.1), density);
	vec3 colCenter = 7.0 * vec3(0.8, 1.0, 1.0);
	vec3 colEdge = 1.5 * vec3(0.48, 0.53, 0.5);
	result *= mix(colCenter, colEdge, min((dist_to_center + 0.05) / 0.9, 1.15));
	return result;
}

// Custom union that handles color blending
vec4 opUnionColored(vec4 d1, vec4 d2, float k) {
	float h = clamp(0.5 + 0.5 * (d2.a - d1.a) / k, 0.0, 1.0);
	float res_d = mix(d2.a, d1.a, h) - k * h * (1.0 - h);
	vec3  res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

// Custom subtraction that handles color for "antimatter" effect
vec4 opSubtractionColored(vec4 d1, vec4 d2, float k) {
	float h = clamp(0.5 - 0.5 * (d2.a + d1.a) / k, 0.0, 1.0);
	float res_d = mix(d2.a, -d1.a, h) + k * h * (1.0 - h);
	vec3 res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

vec3 getFireColor(float heat) {
	heat = clamp(heat, 0.0, 1.0);
	vec3 red = vec3(0.8, 0.0, 0.0);
	vec3 orange = vec3(1.0, 0.4, 0.0);
	vec3 yellow = vec3(1.0, 0.8, 0.1);
	vec3 white = vec3(1.0, 1.0, 0.8);

	if (heat < 0.3)
		return mix(vec3(0.01), red, heat / 0.3);
	if (heat < 0.6)
		return mix(red, orange, (heat - 0.3) / 0.3);
	if (heat < 0.85)
		return mix(orange, yellow, (heat - 0.6) / 0.25);
	return 3.0 * mix(yellow, white, (heat - 0.85) / 0.15);
}

// --- Map Function ---

vec4 map(vec3 p) {
	vec4 res = vec4(1.0, 1.0, 1.0, 1000.0);

	// First pass: Union of positive charges
	bool first = true;
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].params.x > 0.0) {
			float d;
			vec3  col = sources[i].color_smoothness.rgb;

			int type = int(sources[i].params.y);
			if (type == TYPE_EXPLOSION) {
				float alpha_noise = fastWorley3d(p * 0.05 * sources[i].params.w + time * 0.005);
				float noise = fastWarpedFbm3d(p * alpha_noise * sources[i].params.w + time * 0.02);
				noise = mix(noise, alpha_noise, fastSimplex3d(vec3(alpha_noise, noise, noise * alpha_noise)));
				d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
				d += noise * sources[i].params.z;

				float heat = 1.0 - clamp(d / (sources[i].position_radius.w * 0.05), 0.0, 1.0);
				heat = pow(heat, 2.50);
				col = getFireColor(heat * sources[i].params.w + noise * 5.0) * 2.0;
			} else if (type == TYPE_VOLUMETRIC) {
				d = VolumetricExplosion(p, sources[i].position_radius.xyz, sources[i].position_radius.w, sources[i].params.z, sources[i].params.w);
				float density = clamp(-d, 0.0, 1.0);
				col = computeVolumetricColor(density, length(p - sources[i].position_radius.xyz) / sources[i].position_radius.w);
			} else {
				d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			}

			if (first) {
				res = vec4(col, d);
				first = false;
			} else {
				res = opUnionColored(vec4(col, d), res, sources[i].color_smoothness.a);
			}
		}
	}

	// Second pass: Subtraction of negative charges
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].params.x < 0.0) {
			float d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			if (!first) {
				res = opSubtractionColored(
					vec4(sources[i].color_smoothness.rgb, d),
					res,
					sources[i].color_smoothness.a
				);
			}
		}
	}

	return res;
}

vec3 getNormal(vec3 p) {
	vec2 e = vec2(0.01, 0.0);
	return normalize(vec3(
		map(p + e.xyy).a - map(p - e.xyy).a,
		map(p + e.yxy).a - map(p - e.yxy).a,
		map(p + e.yyx).a - map(p - e.yyx).a
	));
}

// --- Main ---

void main() {
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;
	float depth = texture(depthTexture, TexCoords).r;

	// Reconstruct scene world position to get depth limit
	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPos = invProjection * ndcPos;
	viewPos /= viewPos.w;
	vec4  worldPos = invView * viewPos;
	float sceneDistance = length(worldPos.xyz - cameraPos);
	if (depth >= 0.999999)
		sceneDistance = 10000.0;

	// Ray direction
	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

	float t = fastBlueNoise(TexCoords * screenSize);
	vec4  res;
	bool  hit = false;

	bool hasVolumetric = false;
	for (int j = 0; j < numSources; ++j) {
		if (int(sources[j].params.y) == TYPE_VOLUMETRIC) {
			hasVolumetric = true;
			break;
		}
	}

	for (int i = 0; i < 96; ++i) {
		vec3 p = cameraPos + rayDir * t;
		res = map(p);

		if (res.a < 0.01) {
			hit = true;
			break;
		}

		if (hasVolumetric) {
			// Optimized step size for volumetric types
			float stepSize = abs(res.a) + 0.07;
			stepSize = max(stepSize, 0.03);
			t += stepSize * 0.5;
		} else {
			t += res.a;
		}

		if (t > sceneDistance || t > 1500.0)
			break;
	}

	if (hit) {
		vec3  p = cameraPos + rayDir * t;
		vec3  normal = getNormal(p);
		vec3  lightDir = normalize(vec3(0.5, 1.0, 0.5));
		float diff = max(dot(normal, lightDir), 0.0);

		float rim = 1.0 - max(dot(normal, -rayDir), 0.0);
		rim = pow(rim, 3.0);

		vec3 volumeColor = res.rgb * (diff * 0.8 + 0.2) + res.rgb * rim * 0.5;
		FragColor = vec4(mix(sceneColor, volumeColor, 1.0), 1.0);
	} else {
		FragColor = vec4(sceneColor, 1.0);
	}
}
