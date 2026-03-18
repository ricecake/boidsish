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
	vec4 extra_params;     // x: density_cutoff, y: step_size_multiplier
};

layout(std140) uniform SdfVolumes {
	int       numSources;
	int       numNeighbors;
	int       padding[2];
	SdfSource sources[128];
};

#include "lygia/sdf/sphereSDF.glsl"
#include "../helpers/fast_noise.glsl"

#define TYPE_SPHERE 0
#define TYPE_EXPLOSION 1
#define TYPE_VOLUMETRIC 2

// --- Helper Functions ---


float ridged_fBm(vec3 p) {
    float sum = 0.0;
    float amp = 0.5;
    float freq = 1.0;

    for(int i = 0; i < 4; i++) {
        // Assume snoise returns a value from -1.0 to 1.0
        float n = fastSimplex3d(p * freq);

        // 1.0 - abs() creates the sharp crease at the 0-crossing
        n = 1.0 - abs(n);

        // Squaring it narrows the ridges, making them look sharper
        n *= n;

        sum += n * amp;
        freq *= 2.0; // Lacunarity (scale of the next layer)
        amp *= 0.5;  // Gain (influence of the next layer)
    }
    return sum;
}


const float nudge = 4.0;
const float normalizer = 1.0 / sqrt(1.0 + nudge * nudge);

float SpiralNoiseC(vec3 p) {
	// float n = -abs(mod(time * 1.2, 4.0) - 2.0);
	float n = -(sin(time * 1.2) + 1.0);
	// float n = 0.0;//-mod(time, 2.0);
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
	vec3 pos = p - center;

	float max_possible_radius = radius * 1.5;
	float base_d = sphereSDF(pos, max_possible_radius);

	if (base_d > 0.0) {
		return base_d;
	}

	float dist = distance(p, center);
	float d = sphereSDF(pos, radius * (fastWarpedFbm3d(p/30.0+time*0.8*1/radius)*0.65+0.98));

	vec3 warp = fastCurl3d((p+time)/ (10.0*noise_intensity));
	d += (fastFbm3d(p*warp / (10.0*noise_intensity) * d*time*0.5)*0.5+0.5) * smoothstep(0, 0.25, noise_intensity);
	d += ridged_fBm(pos/10.0 * smoothstep(0, 0.5, noise_intensity)+time*0.75);
	d += pow(1-abs(fastWarpedFbm3d(pos/10.0 * 0.6*smoothstep(0, 0.75, dist) + warp*time*0.00005)), 5);
	// d += fastWorley3d(pos/100.0 *smoothstep(0, 0.75, d) + time*0.5);
	return d * 0.5 * distance(pos, center)/radius;
}

vec3 computeVolumetricColor(float density, float dist) {
	vec3 result = mix(vec3(1.0, 0.9, 0.8), vec3(0.4, 0.15, 0.1), smoothstep(0, 0.6, density));
	vec3 colCenter = 6.0 * vec3(0.9, 1.0, 1.0);
	vec3 colEdge = 9.0 * vec3(0.4, 0.2, 0.1);
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

vec4 getOpaqueSDF(vec3 p) {
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

float getVolumetricDistance(vec3 p, out float minStepMultiplier, out float minRadius) {
	float minDist = 1000.0;
	minStepMultiplier = 1.0;
	minRadius = 1.0;
	bool found = false;

	for (int i = 0; i < numSources; ++i) {
		if (int(sources[i].params.y) == TYPE_VOLUMETRIC) {
			float d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			if (d < minDist) {
				minDist = d;
				minStepMultiplier = sources[i].extra_params.y;
				minRadius = sources[i].position_radius.w;
				found = true;
			}
		}
	}
	return found ? minDist : 1000.0;
}

float sampleSingleDensity(vec3 p, int index) {
	SdfSource s = sources[index];
	vec3 pos = p - s.position_radius.xyz;
	float radius = s.position_radius.w;
	float noise_intensity = s.params.z;

	float d = sphereSDF(pos, radius * (fastWarpedFbm3d(p/30.0 + time * 0.8 / radius) * 0.65 + 0.98));
	vec3 warp = fastCurl3d((p + time) / (10.0 * max(0.01, noise_intensity)));
	d += (fastFbm3d(p * warp / (10.0 * max(0.01, noise_intensity)) * d * time * 0.5) * 0.5 + 0.5) * smoothstep(0, 0.25, noise_intensity);
	d += ridged_fBm(pos / 10.0 * smoothstep(0, 0.5, noise_intensity) + time * 0.75);

	float distToCenter = length(pos);
	d += pow(1.0 - abs(fastWarpedFbm3d(pos / 10.0 * 0.6 * smoothstep(0.0, 0.75, distToCenter / radius) + warp * time * 0.00005)), 5.0);

	return clamp(0.2 - d, 0.0, 1.0);
}

void getVolumetricDensity(vec3 p, out vec3 outColor, out float outDensity, out float outMaxDensityCutoff) {
	outColor = vec3(0.0);
	outDensity = 0.0;
	outMaxDensityCutoff = 0.0;
	float totalWeight = 0.0;

	int closestIndices[8];
	float closestDists[8];
	for (int i = 0; i < 8; ++i) closestDists[i] = 1000.0;
	int nFound = 0;
	int nToFind = clamp(numNeighbors, 1, 8);

	for (int i = 0; i < numSources; ++i) {
		if (int(sources[i].params.y) == TYPE_VOLUMETRIC) {
			float d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			if (d < 1.0) {
				int j = nFound;
				while (j > 0 && d < closestDists[j-1]) {
					if (j < nToFind) {
						closestIndices[j] = closestIndices[j-1];
						closestDists[j] = closestDists[j-1];
					}
					j--;
				}
				if (j < nToFind) {
					closestIndices[j] = i;
					closestDists[j] = d;
					if (nFound < nToFind) nFound++;
				}
			}
		}
	}

	for (int i = 0; i < nFound; ++i) {
		int idx = closestIndices[i];
		float d = sampleSingleDensity(p, idx);
		outDensity += d;
		outColor += sources[idx].color_smoothness.rgb * d;
		totalWeight += d;
		outMaxDensityCutoff = max(outMaxDensityCutoff, sources[idx].extra_params.x);
	}

	if (totalWeight > 0.0) {
		outColor /= totalWeight;
		float distToCenter = length(p - sources[closestIndices[0]].position_radius.xyz);
		float normDist = distToCenter / sources[closestIndices[0]].position_radius.w;

		vec3 vCol = computeVolumetricColor(outDensity, normDist);
		outColor = mix(outColor, vCol, 0.5);

		vec3 lightColor = vec3(1.0, 0.6, 0.3);
		outColor += (lightColor / exp(distToCenter * distToCenter * 0.1) / 20.0);
	}
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

	float t = fastBlueNoise(TexCoords * screenSize) * 0.05;
	vec4 sum = vec4(0.0);
	float accumulatedDensity = 0.0;

	for (int i = 0; i < 96; ++i) {
		vec3 p = cameraPos + rayDir * t;

		vec4 resOpaque = getOpaqueSDF(p);
		if (resOpaque.a < 0.01) {
			vec2 e = vec2(0.01, 0.0);
			vec3 normal = normalize(vec3(
				getOpaqueSDF(p + e.xyy).a - getOpaqueSDF(p - e.xyy).a,
				getOpaqueSDF(p + e.yxy).a - getOpaqueSDF(p - e.yxy).a,
				getOpaqueSDF(p + e.yyx).a - getOpaqueSDF(p - e.yyx).a
			));
			vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
			float diff = max(dot(normal, lightDir), 0.0);
			float rim = pow(1.0 - max(dot(normal, -rayDir), 0.0), 3.0);
			vec3 col = resOpaque.rgb * (diff * 0.8 + 0.2) + resOpaque.rgb * rim * 0.5;
			sum = sum + vec4(col, 1.0) * (1.0 - sum.a);
			break;
		}

		float minStepMultiplier, minRadius;
		float dv = getVolumetricDistance(p, minStepMultiplier, minRadius);

		float stepSize = 1.0;

		if (dv < 0.0) {
			vec3 vColor;
			float vDensity, vMaxDensityCutoff;
			getVolumetricDensity(p, vColor, vDensity, vMaxDensityCutoff);

			if (vDensity > 0.0) {
				float weight = vDensity * 0.2;
				accumulatedDensity += weight * (1.0 - sum.a);

				vec4 col = vec4(vColor, vDensity * 0.1);
				col.rgb *= col.a;
				sum = sum + col * (1.0 - sum.a);

				if (accumulatedDensity >= vMaxDensityCutoff) {
					sum.a = 1.0;
					break;
				}
			}
			stepSize = min(resOpaque.a, minRadius * minStepMultiplier);
		} else {
			stepSize = min(resOpaque.a, dv + 0.05);
		}

		t += max(stepSize, 0.02);

		if (t > sceneDistance || t > 1500.0 || sum.a > 0.99) break;
	}

	sum = clamp(sum, 0.0, 1.0);
	sum.rgb = sum.rgb * sum.rgb * (3.0 - 2.0 * sum.rgb);
	FragColor = vec4(mix(sceneColor, sum.rgb, sum.a), 1.0);
}
