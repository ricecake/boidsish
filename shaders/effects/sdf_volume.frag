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

#include "../helpers/sdf_common.glsl"
#include "lygia/lighting/blackbody.glsl"
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

// --- Noise: ridged fBm for detail ---
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
	float noise_intensity = max(0.001, sources[index].volumetric_params.w);

	if (p.y < ground_y) return 0.0;

	vec3 rel = p - center;
	float d = getSdfDistance(p, index);

	// Add some noise-based displacement to the distance field for volume
	vec3 warp = fastCurl3d((p + time * 0.5) / (10.0 * noise_intensity));
	d += (fastFbm3d(p * warp / (10.0 * noise_intensity) * d * 0.1) * 0.5 + 0.5) * smoothstep(0, 0.25, noise_intensity);
	d += fastRidge3d(rel / (10.0 * noise_intensity) * smoothstep(0, 0.5, noise_intensity) + time * 0.75);
    d += fastWorley3d(rel / 100.0 * smoothstep(0, 0.75, d) + time * 0.5);

	if (d > radius * 0.05) return 0.0;

	float normalized_d = clamp(-d / radius, 0.0, 1.0);
	float ground_dist = (p.y - ground_y) / max(radius, 0.01);
	if (ground_dist < 0.3) {
		normalized_d *= 1.0 + 2.5 * smoothstep(0.3, 0.0, ground_dist);
	}

	return max(0.0, normalized_d * sources[index].volumetric_params.x);
}

// --- Temperature-driven color ---
vec3 explosionColor(float normalized_d, float ntime, vec3 color_inner, vec3 color_outer) {
	float temperature = 100 + (40000 * smoothstep(0.0, 0.75, normalized_d * (1.0 - ntime * 0.7)));
	return blackbody(temperature);
}

// --- Multi-source volumetric accumulation ---
void volumetricMarch(
	vec3 rayOrigin, vec3 rayDir, float maxDist,
	out vec3 accumColor, out float transmittance, out bool had_contribution
) {
	accumColor = vec3(0.0);
	transmittance = 1.0;
	had_contribution = false;

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

	float jitter = fastBlueNoise(TexCoords * screenSize * 0.1);

	int num_steps = 64;
	float stepSize = (global_t_end - global_t_start) / float(num_steps);

	for (int j = 0; j < num_steps; ++j) {
		float curT = global_t_start + stepSize * (float(j) + jitter);
		if (curT > maxDist) break;

		vec3 p = rayOrigin + rayDir * curT;

		float totalDensity = 0.0;
		vec3  totalColor = vec3(0.0);
		float totalAbsorption = 0.0;
		float totalWeight = 0.0;

		for (int i = 0; i < numSources; ++i) {
			if (sources[i].charge_type_vol_time.z < 0.5) continue;

			float ntime = sources[i].charge_type_vol_time.w;
            float fader = smoothstep(1.0, 0.85, ntime);
			float d = sampleSourceDensity(p, i) * fader;

			if (d <= 0.0) continue;

			float radius = sources[i].position_radius.w;
			float absorption = sources[i].volumetric_params.y;
            float nd = d; // density from sample

			vec3 col = explosionColor(nd, ntime, sources[i].color_inner.rgb, sources[i].color_outer.rgb);
			float emission = sources[i].color_inner.a;
			float emit = emission * nd * (1.0 - ntime) * fader;
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

// Neighborhood clamping for temporal reprojection
vec3 clipHistory(vec3 history, vec2 currentTexCoords) {
    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    vec2 tSize = 1.0 / screenSize;

    // We need to sample the current frame's output in the neighborhood.
    // However, the current frame's output is what we're currently calculating.
    // In this specific pass, we can at least clamp against the background scene
    // AND the local SDF contribution to avoid massive ghosting trails.
    // For a simple post-process pass like this, clamping against the background is
    // actually often what causes ghosting to disappear but at the cost of the effect.

    // A better approach here is to NOT use neighborhood clamping if we can't
    // sample the current finished result, OR to calculate the local neighborhood
    // of the SDF effect itself.

    // Given the constraints, let's skip neighborhood clamping and use a
    // standard reprojection with a tighter blend if motion is high.
    return history;
}

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

	float t = 0.0;
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

	vec3  volAccumColor;
	float transmittance;
	bool  vol_contribution;
	volumetricMarch(cameraPos, rayDir, sceneDistance, volAccumColor, transmittance, vol_contribution);

	if (vol_contribution)
		had_sdf_contribution = true;

	if (hit_surface && t_surface < sceneDistance) {
		vec3  p = cameraPos + rayDir * t_surface;
		vec3  normal = getNormal(p);
		vec3  lightDir = normalize(vec3(0.5, 1.0, 0.5));
		float diff = max(dot(normal, lightDir), 0.0);
		float rim = pow(1.0 - max(dot(normal, -rayDir), 0.0), 3.0);

		vec4 res = mapColor(p);
		vec3 surfaceColor = res.rgb * (diff * 0.8 + 0.2) + res.rgb * rim * 0.5;
		currentFrameColor = volAccumColor + transmittance * surfaceColor;
	} else {
		currentFrameColor = volAccumColor + transmittance * sceneColor;
	}

	if (!had_sdf_contribution) {
		FragColor = vec4(currentFrameColor, 1.0);
	} else {
		vec3 reprojWorldPos = (hit_surface && t_surface < sceneDistance)
			? cameraPos + rayDir * t_surface
			: worldPos.xyz;
		vec4 reprojectedPos = prevViewProjection * vec4(reprojWorldPos, 1.0);
		vec2 prevTexCoords = (reprojectedPos.xy / reprojectedPos.w) * 0.5 + 0.5;

		vec4 historySample = texture(historyTexture, prevTexCoords);
        vec3 historyColor = historySample.rgb;

		bool onScreen = prevTexCoords.x >= 0.0 && prevTexCoords.x <= 1.0 &&
		                prevTexCoords.y >= 0.0 && prevTexCoords.y <= 1.0;

		float baseBlend = (hit_surface && t_surface < sceneDistance) ? 0.9 : 0.4;
		float blendFactor = (onScreen && frameIndex > 0) ? baseBlend : 0.0;

		FragColor = mix(vec4(currentFrameColor, 1.0), vec4(historyColor, 1.0), blendFactor);
	}
}
