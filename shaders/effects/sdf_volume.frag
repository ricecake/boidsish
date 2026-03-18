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
	// float d = sphereSDF((p - center)*noise_scale, radius * smoothstep(0, 0.5, noise_intensity));
	float d = sphereSDF((p - center), radius);

	d += fastWarpedFbm3d(p*fastWorley3d(p*noise_intensity/(20.0*radius)) / (10.0*radius) * noise_scale) * smoothstep(0, 0.5, noise_intensity);
	d += SpiralNoiseC((p - center) * 0.6*smoothstep(0, 0.8, noise_intensity) + 333.0);
	return d;
}

vec3 computeVolumetricColor(float density, float dist) {
	// vec3 result = mix(vec3(1.0, 0.9, 0.8), vec3(0.4, 0.15, 0.1), clamp(density, 0.0, 1.0));
	vec3 result = mix(vec3(1.0, 0.9, 0.8), vec3(0.4, 0.15, 0.1), smoothstep(0, 0.6, density));
	vec3 colCenter = 15.0 * vec3(0.9, 1.0, 1.0);
	vec3 colEdge = 8.0 * vec3(0.4, 0.2, 0.1);
	result *= mix(colCenter, colEdge, clamp(dist, 0.0, 1.0));
	return result;
}

vec4 opUnionColored(vec4 d1, vec4 d2, float k) {
	float h = clamp(0.5 + 0.5 * (d2.a - d1.a) / k, 0.0, 1.0);
	float res_d = mix(d2.a, d1.a, h) - k * h * (1.0 - h);
	vec3  res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

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

vec4 mapOpaque(vec3 p) {
	vec4 res = vec4(1.0, 1.0, 1.0, 1000.0);
	bool first = true;
	for (int i = 0; i < numSources; ++i) {
		int type = int(sources[i].params.y);
		if (type != TYPE_VOLUMETRIC) {
			float d;
			vec3 col = sources[i].color_smoothness.rgb;
			if (type == TYPE_EXPLOSION) {
				float alpha_noise = fastWorley3d(p * 0.05 * sources[i].params.w + time * 0.005);
				float noise = fastWarpedFbm3d(p * alpha_noise * sources[i].params.w + time * 0.02);
				noise = mix(noise, alpha_noise, fastSimplex3d(vec3(alpha_noise, noise, noise * alpha_noise)));
				d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
				d += noise * sources[i].params.z;
				float heat = 1.0 - clamp(d / (sources[i].position_radius.w * 0.05), 0.0, 1.0);
				heat = pow(heat, 2.50);
				col = getFireColor(heat * sources[i].params.w + noise * 5.0) * 2.0;
			} else {
				d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			}

			if (sources[i].params.x > 0.0) {
				if (first) { res = vec4(col, d); first = false; }
				else { res = opUnionColored(vec4(col, d), res, sources[i].color_smoothness.a); }
			} else {
				if (!first) { res = opSubtractionColored(vec4(col, d), res, sources[i].color_smoothness.a); }
			}
		}
	}
	return res;
}

void main() {
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;
	float depth = texture(depthTexture, TexCoords).r;
	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPos = invProjection * ndcPos;
	viewPos /= viewPos.w;
	vec4  worldPos = invView * viewPos;
	float sceneDistance = length(worldPos.xyz - cameraPos);
	if (depth >= 0.999999) sceneDistance = 10000.0;
	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

	float t = fastBlueNoise(TexCoords * screenSize) * 0.1;
	vec4 sum = vec4(0.0);
	float td = 0.0;

	for (int i = 0; i < 96; ++i) {
		vec3 p = cameraPos + rayDir * t;

		vec4 resOpaque = mapOpaque(p);
		if (resOpaque.a < 0.01) {
			vec2 e = vec2(0.01, 0.0);
			vec3 normal = normalize(vec3(
				mapOpaque(p + e.xyy).a - mapOpaque(p - e.xyy).a,
				mapOpaque(p + e.yxy).a - mapOpaque(p - e.yxy).a,
				mapOpaque(p + e.yyx).a - mapOpaque(p - e.yyx).a
			));
			vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
			float diff = max(dot(normal, lightDir), 0.0);
			float rim = pow(1.0 - max(dot(normal, -rayDir), 0.0), 3.0);
			vec3 col = resOpaque.rgb * (diff * 0.8 + 0.2) + resOpaque.rgb * rim * 0.5;
			sum = sum + vec4(col, 1.0) * (1.0 - sum.a);
			break;
		}

		float minVolD = 1000.0;
		int nearestVol = -1;
		for (int k = 0; k < numSources; ++k) {
			if (int(sources[k].params.y) == TYPE_VOLUMETRIC) {
				float dVol = VolumetricExplosion(p, sources[k].position_radius.xyz, sources[k].position_radius.w, sources[k].params.z, sources[k].params.w);
				if (dVol < minVolD) {
					minVolD = dVol;
					nearestVol = k;
				}
			}
		}

		float dv = 1000.0;
		if (nearestVol != -1) {
			dv = minVolD; // Use raw distance for volumetric
			if (dv < 0.2) {
				float h = 0.2;
				float ld = h - dv;
				float w = (1.0 - sum.a) * ld;
				td += w + 1.0 / 150.0;

				float distToCenter = length(p - sources[nearestVol].position_radius.xyz);
				float normDist = distToCenter / sources[nearestVol].position_radius.w;
				vec4 col = vec4(computeVolumetricColor(td, normDist), td);

				vec3 lightColor = vec3(1.0, 0.6, 0.3);
				sum.rgb += (lightColor / exp(distToCenter * distToCenter * 0.1) / 20.0) * (1.0 - sum.a);
				sum.rgb += sum.a * sum.rgb * 0.2 / max(distToCenter, 0.01);

				col.a *= 0.2;
				col.rgb *= col.a;
				sum = sum + col * (1.0 - sum.a);
			}
			dv = abs(dv) + 0.05;
		}

		float stepSize = min(resOpaque.a, max(dv, 0.02) * 0.6);
		t += stepSize;

		if (t > sceneDistance || t > 1500.0 || sum.a > 0.99) break;
	}

	sum = clamp(sum, 0.0, 1.0);
	sum.rgb = sum.rgb * sum.rgb * (3.0 - 2.0 * sum.rgb);
	FragColor = vec4(mix(sceneColor, sum.rgb, sum.a), 1.0);
}
