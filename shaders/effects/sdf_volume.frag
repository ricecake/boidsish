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
	vec4 extra_params;           // x: refraction_strength, yzw: unused
};

layout(std430, binding = [[SDF_VOLUMES_BINDING]]) buffer SdfVolumes {
	int       numSources;
	SdfSource sources[];
};

#include "lygia/sdf/sphereSDF.glsl"
#include "lygia/lighting/blackbody.glsl"
#include "../helpers/fast_noise.glsl"
#include "helpers/noise.glsl"

layout(std140, binding = [[TEMPORAL_DATA_BINDING]]) uniform TemporalData {
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

// --- Shape helper ---
float smin( float a, float b, float k )
{
    float h = max(k-abs(a-b),0.0);
    return min(a, b) - h*h*0.25/k;
}

float cylinderSDF( vec3 p, float h, float r ) {
    vec2 d = abs(vec2(length(p.xz),p.y)) - vec2(h,r);
    return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}

float sdCappedCone(vec3 p, vec2 c) {
    vec2 q = vec2(length(p.xz), p.y);
    float d = dot(q, c);
    return length(q - c * max(min(d, 1.0), 0.0)) * sign(q.y - c.y);
}

float mushroomSDF(vec3 p, float radius, float ntime) {
    p.y -= radius;
    float h = clamp((p.y + radius) / (2.0 * radius), 0.0, 1.0);
    float cap_flare = smoothstep(0.4, 0.9, h)*1.43;
    float base_flare = smoothstep(0.3, 0.0, h);
    float base_width_over_time = mix(0.0, 2.5, ntime);
    float target_pinch = 0.2 + cap_flare + (base_flare * base_width_over_time);
    float pinch = mix(1.0, target_pinch, smoothstep(0.0, 0.6, ntime));
    vec3 warped = p;
    warped.xz /= pinch;
    return ((length(warped) - radius) * 0.4);
}

float getSourceDistance(vec3 p, int i) {
	vec3 rel = p - sources[i].position_radius.xyz;
	float radius = sources[i].position_radius.w;
	int type = int(sources[i].charge_type_vol_time.y);

	if (type == 1) { // Mushroom
		float ntime = sources[i].charge_type_vol_time.w;
		return mushroomSDF(rel, radius, ntime);
	} else { // Default: Sphere (type 0)
		return sphereSDF(rel, radius);
	}
}

// --- Opaque SDF Surface Functions ---

float mapDistance(vec3 p) {
	float d = 1e10;
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x > 0.0 && sources[i].charge_type_vol_time.z < 0.5) {
			float d_src = getSourceDistance(p, i);
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
			float d_src = getSourceDistance(p, i);
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
			float d = getSourceDistance(p, i);
			if (first) { res = vec4(sources[i].color_smoothness.rgb, d); first = false; }
			else { res = opUnionColored(vec4(sources[i].color_smoothness.rgb, d), res, sources[i].color_smoothness.a); }
		}
	}
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x < 0.0) {
			float d = getSourceDistance(p, i);
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

// --- Per-source volumetric density with rich noise ---

float sampleSourceDensity(vec3 p, int index) {
	vec3  center = sources[index].position_radius.xyz;
	float radius = sources[index].position_radius.w;
	float ground_y = sources[index].color_outer.a;
	float ntime = sources[index].charge_type_vol_time.w;
	float noise_scale = sources[index].volumetric_params.z;
	float noise_intensity = sources[index].volumetric_params.w;

	if (p.y < ground_y) return 0.0;

	float d = getSourceDistance(p, index);
	// Apply noise warp to boundary
	float boundary_noise = fastWarpedFbm3d(p/30.0+time*0.8*1/radius)*0.65+0.98;
	d /= boundary_noise;

	if (d > radius * 0.05) return 0.0;

	float normalized_d = clamp(-d / radius, 0.0, 1.0);
	vec3 rel = p - center;
	vec3 warp = fastCurl3d((p+time)/ (10.0*noise_intensity));

	float noise = 0.0;
	noise += (fastFbm3d(p*warp / (10.0*noise_intensity) * d*time*0.5)*0.5+0.5) * smoothstep(0, 0.25, noise_intensity);
	noise += fastRidge3d(rel/(10.0*noise_intensity) * smoothstep(0, 0.5, noise_intensity)+time*0.75);
    noise += fastWorley3d(rel/100.0 *smoothstep(0, 0.75, d) + time*0.5);

	float density = normalized_d + noise;

	float ground_dist = (p.y - ground_y) / max(radius, 0.01);
	if (ground_dist < 0.3) {
		density *= 1.0 + 2.5 * smoothstep(0.3, 0.0, ground_dist);
	}

	return max(0.0, density);
}

// --- Temperature-driven color ---

vec3 explosionColor(float normalized_d, float ntime, vec3 color_inner, vec3 color_outer) {
	float temperature = 100+(40000 * smoothstep(0.0, 0.75, normalized_d * (1.0 - ntime * 0.7)));
	return blackbody(temperature);
}

// --- Multi-source volumetric accumulation ---

float getRefractionF(vec3 p) {
	float f = 0.0;
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.z < 0.5) continue;
		float strength = sources[i].extra_params.x;
		if (abs(strength) < 1e-6) continue;

		float radius = sources[i].position_radius.w;
		float ntime = sources[i].charge_type_vol_time.w;
		float d = getSourceDistance(p, i);
		f += clamp(-d / radius, 0.0, 1.0) * strength * smoothstep(1.0, 0.85, ntime);
	}
	return f;
}

void volumetricMarch(
	vec3 rayOrigin, vec3 rayDir, float maxDist,
	out vec3 accumColor, out float transmittance, out bool had_contribution,
	out vec3 refractedRayDir, out vec3 refractedRayPos
) {
	accumColor = vec3(0.0);
	transmittance = 1.0;
	had_contribution = false;
	refractedRayDir = rayDir;
	refractedRayPos = rayOrigin;

	// Find the union of all volumetric bounding intervals along the ray
	float global_t_start = 1e10;
	float global_t_end = -1e10;

	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.z < 0.5) continue;

		vec3  center = sources[i].position_radius.xyz;
		float bound_radius = sources[i].position_radius.w * 4.0;

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

	int   num_steps = 56;
	float totalDist = global_t_end - global_t_start;
	float stepSize = totalDist / float(num_steps);

	vec3  p = rayOrigin + rayDir * (global_t_start + jitter * stepSize);
	vec3  currentRayDir = rayDir;

	for (int j = 0; j < num_steps; ++j) {
		float totalDensity = 0.0;
		vec3  totalColor = vec3(0.0);
		float totalAbsorption = 0.0;
		float totalWeight = 0.0;

		float f = getRefractionF(p);

		for (int i = 0; i < numSources; ++i) {
			if (sources[i].charge_type_vol_time.z < 0.5) continue;

			float ntime = sources[i].charge_type_vol_time.w;
			float fader = smoothstep(1.0, 0.85, ntime);

			float d = sampleSourceDensity(p, i) * fader;

			if (d <= 0.0) continue;

			float radius = sources[i].position_radius.w;
			float absorption = sources[i].volumetric_params.y;
			float emission = sources[i].color_inner.a;

			float md = getSourceDistance(p, i);
			float nd = d * clamp(-md / radius, 0.0, 1.0);

			vec3  col = explosionColor(nd, ntime, sources[i].color_inner.rgb, sources[i].color_outer.rgb);
			float emit = emission * nd * (1.0 - ntime) * fader;
			col += col * emit;

			totalDensity += d * fader;
			totalColor += col * d * fader;
			totalAbsorption += absorption * d * fader;
			totalWeight += d * fader;
		}

		// Refraction update
		if (abs(f) > 1e-4) {
			float e = 0.1;
			vec3  gradF = vec3(
				getRefractionF(p + vec3(e, 0, 0)) - f,
				getRefractionF(p + vec3(0, e, 0)) - f,
				getRefractionF(p + vec3(0, 0, e)) - f
			) / e;

			float n = 1.0 + f;
			currentRayDir = normalize(currentRayDir * n + gradF * stepSize);
		}

		if (totalWeight > 0.0) {
			totalColor /= totalWeight;
			totalAbsorption /= totalWeight;

			float alpha = 1.0 - exp(-totalDensity * stepSize);
			accumColor += transmittance * alpha * totalColor;
			transmittance *= exp(-totalAbsorption * totalDensity * stepSize);
			had_contribution = true;
		}

		p += currentRayDir * stepSize;

		if (length(p - cameraPos) > maxDist)
			break;
		if (transmittance < 0.01)
			break;
	}

	refractedRayDir = currentRayDir;
	refractedRayPos = p;
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

	// --- Part 1: Unified Volumetric March (all sources at once) ---
	vec3  volAccumColor;
	float transmittance;
	bool  vol_contribution;
	vec3  refractedRayDir, refractedRayPos;
	volumetricMarch(cameraPos, rayDir, sceneDistance, volAccumColor, transmittance, vol_contribution, refractedRayDir, refractedRayPos);

	// --- Part 2: Sphere Tracing for Opaque Surfaces (using refracted ray if applicable) ---
	float t = 0.0;
	vec4  res;
	bool  hit_surface = false;

	// If we had a lot of refraction, we should ideally march from the start with refraction
	// But for opaque surfaces we'll just check if we hit anything along the FINAL refracted path
	// to simplify and ensure background refraction looks correct.

	vec3 traceOrigin = cameraPos;
	vec3 traceDir = rayDir;

	if (vol_contribution) {
		// Use refracted ray for surface test
		traceDir = refractedRayDir;
		// Position might have shifted, but starting from camera avoids gaps
	}

	for (int i = 0; i < 96; ++i) {
		vec3 p = traceOrigin + traceDir * t;
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
	bool  had_sdf_contribution = vol_contribution || (hit_surface && t_surface < sceneDistance);

	// Composite
	if (hit_surface && t_surface < sceneDistance) {
		vec3  p = traceOrigin + traceDir * t_surface;
		vec3  normal = getNormal(p);
		vec3  lightDir = normalize(vec3(0.5, 1.0, 0.5));
		float diff = max(dot(normal, lightDir), 0.0);
		float rim = pow(1.0 - max(dot(normal, -traceDir), 0.0), 3.0);

		res = mapColor(p);
		vec3  surfaceColor = res.rgb * (diff * 0.8 + 0.2) + res.rgb * rim * 0.5;
		currentFrameColor = volAccumColor + transmittance * surfaceColor;
	} else {
		// Use refracted ray for background
		vec3  finalBackground = sceneColor;
		if (vol_contribution && transmittance > 0.01) {
			// Project the refracted ray hit onto the background plane/scene depth
			vec3  distortedHitPos = refractedRayPos + refractedRayDir * max(0.0, sceneDistance - length(refractedRayPos - cameraPos));
			vec4  clipPos = viewProjection * vec4(distortedHitPos, 1.0);
			vec2  distortedUV = (clipPos.xy / clipPos.w) * 0.5 + 0.5;
			if (distortedUV.x >= 0.0 && distortedUV.x <= 1.0 && distortedUV.y >= 0.0 && distortedUV.y <= 1.0) {
				finalBackground = texture(sceneTexture, distortedUV).rgb;
			}
		}
		currentFrameColor = volAccumColor + transmittance * finalBackground;
	}

	// --- Part 3: Temporal Reprojection (SDF pixels only) ---
	if (!had_sdf_contribution) {
		FragColor = vec4(currentFrameColor, 1.0);
	} else {
		vec3 reprojWorldPos = (hit_surface && t_surface < sceneDistance)
			? traceOrigin + traceDir * t_surface
			: worldPos.xyz;
		vec4 reprojectedPos = prevViewProjection * vec4(reprojWorldPos, 1.0);
		vec2 prevTexCoords = (reprojectedPos.xy / reprojectedPos.w) * 0.5 + 0.5;

		vec4 historyColor = texture(historyTexture, prevTexCoords);

		bool onScreen = prevTexCoords.x >= 0.0 && prevTexCoords.x <= 1.0 &&
		                prevTexCoords.y >= 0.0 && prevTexCoords.y <= 1.0;

		float baseBlend = (hit_surface && t_surface < sceneDistance) ? 0.85 : 0.15;
		float blendFactor = (onScreen && frameIndex > 0) ? baseBlend : 0.0;

		FragColor = mix(vec4(currentFrameColor, 1.0), historyColor, blendFactor);
	}
}
