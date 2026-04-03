#version 430 core

layout(location = 0) out vec4 FragColor;
in vec2  TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D historyTexture;
uniform vec2      screenSize;
uniform vec3      cameraPos;
uniform mat4      invView;
uniform mat4      invProjection;
uniform float     time;

struct SdfSource {
	vec4 position_radius;        // xyz: pos, w: radius
	vec4 color_smoothness;       // rgb: color, a: smoothness
	vec4 charge_type_vol_time;   // x: charge, y: type, z: volumetric, w: normalized_time (0-1)
	vec4 volumetric_params;      // x: density, y: absorption, z: noise_scale, w: noise_intensity
	vec4 color_inner;            // rgb: inner color, a: emission intensity
	vec4 color_outer;            // rgb: outer color, a: ground_y
};

layout(std430, binding = 25) buffer SdfVolumes {
	int       numSources;
	SdfSource sources[];
};

#include "lygia/sdf/sphereSDF.glsl"
#include "../helpers/fast_noise.glsl"
#include "helpers/noise.glsl"

layout(std140, binding = 6) uniform TemporalData {
	mat4  viewProjection;
	mat4  prevViewProjection;
	mat4  uProjection;
	mat4  invProjectionTemporal;
	mat4  invViewTemporal;
	vec2  texelSize;
	int   frameIndex;
	float padding_temporal;
};

// --- Noise from earlier shader: ridged fBm for sharp creases ---
float ridgedFbm(vec3 p) {
	float sum = 0.0;
	float amp = 0.5;
	float freq = 1.0;
	for (int i = 0; i < 4; i++) {
		float n = fastSimplex3d(p * freq);
		n = 1.0 - abs(n);
		n *= n;
		sum += n * amp;
		freq *= 2.0;
		amp *= 0.5;
	}
	return sum;
}

// --- SDF Operations ---

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

// --- Opaque SDF Surface Functions ---

float mapDistance(vec3 p) {
	float d = 1e10;
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x > 0.0 && sources[i].charge_type_vol_time.z < 0.5) {
			float d_src = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			if (d > 1e9) d = d_src;
			else {
				float k = sources[i].color_smoothness.a;
				float h = clamp(0.5 + 0.5 * (d - d_src) / k, 0.0, 1.0);
				d = mix(d, d_src, h) - k * h * (1.0 - h);
			}
		}
	}
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x < 0.0) {
			float d_src = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			float k = sources[i].color_smoothness.a;
			float h = clamp(0.5 - 0.5 * (d + d_src) / k, 0.0, 1.0);
			d = mix(d, -d_src, h) + k * h * (1.0 - h);
		}
	}
	return d;
}

vec4 mapColor(vec3 p) {
	vec4 res = vec4(1.0, 1.0, 1.0, 1e10);
	bool first = true;
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x > 0.0 && sources[i].charge_type_vol_time.z < 0.5) {
			float d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			if (first) { res = vec4(sources[i].color_smoothness.rgb, d); first = false; }
			else { res = opUnionColored(vec4(sources[i].color_smoothness.rgb, d), res, sources[i].color_smoothness.a); }
		}
	}
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x < 0.0) {
			float d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			if (!first) { res = opSubtractionColored(vec4(sources[i].color_smoothness.rgb, d), res, sources[i].color_smoothness.a); }
		}
	}
	return res;
}

vec3 getNormal(vec3 p) {
	vec2 e = vec2(0.01, 0.0);
	return normalize(vec3(
		mapDistance(p + e.xyy) - mapDistance(p - e.xyy),
		mapDistance(p + e.yxy) - mapDistance(p - e.yxy),
		mapDistance(p + e.yyx) - mapDistance(p - e.yyx)
	));
}

// --- Mushroom-shaped SDF ---

float mushroomSDF(vec3 rel, float radius, float ntime) {
	float elongation = mix(1.2, 1.8, ntime);
	vec3 warped = rel;
	warped.y /= elongation;

	float height_frac = clamp((warped.y / radius) + 0.5, 0.0, 1.0);
	float xz_scale = mix(0.4, 1.2, smoothstep(0.15, 0.6, height_frac));
	warped.xz /= xz_scale;

	return sphereSDF(warped, radius);
}

// --- Per-source volumetric density with rich noise ---

float sampleSourceDensity(vec3 p, int index) {
	vec3  center = sources[index].position_radius.xyz;
	float radius = sources[index].position_radius.w;
	float ground_y = sources[index].color_outer.a;
	float ntime = sources[index].charge_type_vol_time.w;
	float noise_scale = sources[index].volumetric_params.z;
	float noise_intensity = sources[index].volumetric_params.w;

	if (p.y < ground_y) return 0.0;

	vec3 rel = p - center;
	float d = mushroomSDF(rel, radius, ntime);
	if (d > radius * 0.05) return 0.0;

	float normalized_d = clamp(-d / radius, 0.0, 1.0);

	// Height profile: dense cap, thinner stem
	float height_frac = clamp((rel.y / radius) + 0.5, 0.0, 1.0);
	float cap_density = smoothstep(0.0, 0.25, height_frac) * (0.3 + 0.7 * smoothstep(0.35, 0.75, height_frac));

	float density = sources[index].volumetric_params.x * normalized_d * cap_density;

	// Rich noise stack from the earlier shader:
	// 1. Curl-warped FBM for large-scale billowing displacement
	vec3 noise_p = p * noise_scale;
	vec3 warp = fastCurl3d((p + time * 0.5) / (10.0 * max(0.01, noise_intensity)));
	float warped_fbm = fastWarpedFbm3d(noise_p * 0.1 + warp * 0.3 + vec3(0.0, -time * 0.3, 0.0));

	// 2. Ridged FBm for sharp crease detail
	float ridges = ridgedFbm(noise_p * 0.15 + time * 0.4);

	// 3. Base FBm for softer variation
	float base_fbm = fastFbm3d(noise_p * 0.08 + vec3(0.0, -time * 0.5, 0.0)) * 0.5 + 0.5;

	// Combine: ridges give definition, warped fbm gives large-scale structure
	density *= mix(0.2, 2.0, ridges) * mix(0.4, 1.4, base_fbm);
	density += density * warped_fbm * noise_intensity * 0.5;

	// Soft edges
	density *= smoothstep(0.0, 0.12, normalized_d);

	// Ground interaction: rolling dense base
	float ground_dist = (p.y - ground_y) / max(radius, 0.01);
	if (ground_dist < 0.3) {
		density *= 1.0 + 2.5 * smoothstep(0.3, 0.0, ground_dist);
	}

	return max(0.0, density);
}

// --- Temperature-driven color ---

vec3 explosionColor(float normalized_d, float ntime, vec3 color_inner, vec3 color_outer) {
	float temperature = normalized_d * (1.0 - ntime * 0.7);

	vec3 white_hot = vec3(1.0, 0.95, 0.8);
	vec3 yellow    = vec3(1.0, 0.8, 0.2);
	vec3 orange    = color_inner;
	vec3 red       = color_outer;
	vec3 smoke     = vec3(0.15, 0.1, 0.08);

	vec3 col;
	if (temperature > 0.66)
		col = mix(orange, white_hot, (temperature - 0.8) / 0.2);
	else if (temperature > 0.33)
		col = mix(yellow, orange, (temperature - 0.5) / 0.3);
	else
		col = mix(red, yellow, (temperature - 0.25) / 0.25);

	return col;
}

// --- Multi-source volumetric accumulation ---
// Finds the volumetric bounding interval along the ray, then marches once,
// accumulating density from ALL nearby sources at each step point.

void volumetricMarch(
	vec3 rayOrigin, vec3 rayDir, float maxDist,
	out vec3 accumColor, out float transmittance, out bool had_contribution
) {
	accumColor = vec3(0.0);
	transmittance = 1.0;
	had_contribution = false;

	// Find the union of all volumetric bounding intervals along the ray
	float global_t_start = 1e10;
	float global_t_end = -1e10;

	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.z < 0.5) continue;

		vec3  center = sources[i].position_radius.xyz;
		float bound_radius = sources[i].position_radius.w * 2.0;

		vec3  co = rayOrigin - center;
		float b_dot = dot(rayDir, co);
		float c_det = dot(co, co) - bound_radius * bound_radius;
		float det = b_dot * b_dot - c_det;

		if (det > 0.0) {
			float sq = sqrt(det);
			float t1 = max(0.0, -b_dot - sq);
			float t2 = min(maxDist, -b_dot + sq);
			if (t1 < t2) {
				global_t_start = min(global_t_start, t1);
				global_t_end = max(global_t_end, t2);
			}
		}
	}

	if (global_t_start >= global_t_end) return;
	global_t_end = min(global_t_end, maxDist);

	// Blue noise jitter to reduce banding
	float jitter = fastBlueNoise(TexCoords * screenSize * 0.1);

	int num_steps = 56;
	float stepSize = (global_t_end - global_t_start) / float(num_steps);

	for (int j = 0; j < num_steps; ++j) {
		float curT = global_t_start + stepSize * (float(j) + jitter);
		if (curT > maxDist) break;

		vec3 p = rayOrigin + rayDir * curT;

		// Accumulate density and color from ALL volumetric sources at this point
		float totalDensity = 0.0;
		vec3  totalColor = vec3(0.0);
		float totalEmission = 0.0;
		float totalAbsorption = 0.0;
		float totalWeight = 0.0;

		for (int i = 0; i < numSources; ++i) {
			if (sources[i].charge_type_vol_time.z < 0.5) continue;

			float ntime = sources[i].charge_type_vol_time.w;

            float fader = smoothstep(1.0, 0.85, ntime);

			float d = sampleSourceDensity(p, i) * fader;

			if (d <= 0.0) continue;

			float radius = sources[i].position_radius.w;
			vec3  center = sources[i].position_radius.xyz;
			float emission = sources[i].color_inner.a;
			float absorption = sources[i].volumetric_params.y;

			float md = mushroomSDF(p - center, radius, ntime);
			float nd = clamp(-md / radius, 0.0, 1.0);

			vec3 col = explosionColor(nd, ntime, sources[i].color_inner.rgb, sources[i].color_outer.rgb) * fader;
			float emit = emission * nd * (1.0 - ntime);
			col += col * emit;

			totalDensity += d;
			totalColor += col * d;
			totalAbsorption += absorption * d;
			totalWeight += d;
		}

		if (totalWeight <= 0.0) continue;

		totalColor /= totalWeight;
		totalAbsorption /= totalWeight;

		float alpha = 1.0 - exp(-totalDensity * stepSize);
		accumColor += transmittance * alpha * totalColor;
		transmittance *= exp(-totalAbsorption * totalDensity * stepSize);
		had_contribution = true;

		if (transmittance < 0.01) break;
	}
}

// =============================================================================

void main() {
	vec4 sceneColorSample = texture(sceneTexture, TexCoords);
	vec3 sceneColor = sceneColorSample.rgb;
	float depth = texture(depthTexture, TexCoords).r;

	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPos = invProjection * ndcPos;
	viewPos /= viewPos.w;
	vec4  worldPos = invView * viewPos;
	float sceneDistance = length(worldPos.xyz - cameraPos);
	if (depth >= 0.999999)
		sceneDistance = 10000.0;

	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

	// --- Part 1: Sphere Tracing for Opaque Surfaces ---
	float t = 0.0;
	vec4  res;
	bool  hit_surface = false;

	for (int i = 0; i < 96; ++i) {
		vec3 p = cameraPos + rayDir * t;
		float d = mapDistance(p);
		if (d < 0.01) {
			hit_surface = true;
			break;
		}
		t += d;
		if (t > sceneDistance || t > 2000.0)
			break;
	}

	vec3  currentFrameColor = sceneColor;
	float t_surface = t;
	bool  had_sdf_contribution = hit_surface && t_surface < sceneDistance;

	// --- Part 2: Unified Volumetric March (all sources at once) ---
	vec3  volAccumColor;
	float transmittance;
	bool  vol_contribution;
	volumetricMarch(cameraPos, rayDir, sceneDistance, volAccumColor, transmittance, vol_contribution);

	if (vol_contribution)
		had_sdf_contribution = true;

	// Composite: volumetric in front of surfaces/scene
	if (hit_surface && t_surface < sceneDistance) {
		vec3  p = cameraPos + rayDir * t_surface;
		vec3  normal = getNormal(p);
		vec3  lightDir = normalize(vec3(0.5, 1.0, 0.5));
		float diff = max(dot(normal, lightDir), 0.0);
		float rim = pow(1.0 - max(dot(normal, -rayDir), 0.0), 3.0);

		res = mapColor(p);
		vec3 surfaceColor = res.rgb * (diff * 0.8 + 0.2) + res.rgb * rim * 0.5;
		currentFrameColor = volAccumColor + transmittance * surfaceColor;
	} else {
		currentFrameColor = volAccumColor + transmittance * sceneColor;
	}

	// --- Part 3: Temporal Reprojection (SDF pixels only) ---
	if (!had_sdf_contribution) {
		FragColor = vec4(currentFrameColor, 1.0);
	} else {
		vec3 reprojWorldPos = (hit_surface && t_surface < sceneDistance)
			? cameraPos + rayDir * t_surface
			: worldPos.xyz;
		vec4 reprojectedPos = prevViewProjection * vec4(reprojWorldPos, 1.0);
		vec2 prevTexCoords = (reprojectedPos.xy / reprojectedPos.w) * 0.5 + 0.5;

		vec4 historyColor = texture(historyTexture, prevTexCoords);

		bool onScreen = prevTexCoords.x >= 0.0 && prevTexCoords.x <= 1.0 &&
		                prevTexCoords.y >= 0.0 && prevTexCoords.y <= 1.0;

		// Lower blend for volumetric so turbulence detail comes through
		float baseBlend = (hit_surface && t_surface < sceneDistance) ? 0.85 : 0.55;
		float blendFactor = (onScreen && frameIndex > 0) ? baseBlend : 0.0;

		FragColor = mix(vec4(currentFrameColor, 1.0), historyColor, blendFactor);
	}
}
