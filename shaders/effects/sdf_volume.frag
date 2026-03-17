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
	// Blend predator color into the blob where it's being "eaten"
	vec3 res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

#define TYPE_SPHERE 0
#define TYPE_EXPLOSION 1

vec3 getFireColor(float heat) {
	heat = clamp(heat, 0.0, 1.0);
	vec3 red = vec3(0.8, 0.0, 0.0);
	vec3 orange = vec3(1.0, 0.4, 0.0);
	vec3 yellow = vec3(1.0, 0.8, 0.1);
	vec3 white = vec3(1.0, 1.0, 0.8);

	// Shifted heat thresholds to favor orange/red
	if (heat < 0.3)
		return mix(vec3(0.01), red, heat / 0.3);
	if (heat < 0.6)
		return mix(red, orange, (heat - 0.3) / 0.3);
	if (heat < 0.85)
		return mix(orange, yellow, (heat - 0.6) / 0.25);
	return mix(yellow, white, (heat - 0.85) / 0.15);
}

vec4 map(vec3 p) {
	vec4 res = vec4(1.0, 1.0, 1.0, 1000.0);

	// First pass: Union of positive charges (Boids)
	bool first = true;
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].params.x > 0.0) {
			float d;
			vec3  col = sources[i].color_smoothness.rgb;

			if (int(sources[i].params.y) == TYPE_EXPLOSION) {
				float noise = fastFbm3d(p * sources[i].params.w + time * 0.2);
				d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
				d += noise * sources[i].params.z;

				float heat = 1.0 - clamp(d / (sources[i].position_radius.w * 0.5), 0.0, 1.0);
				heat = pow(heat, 2.0); // Sharper falloff

				// Apply a second noise for alpha/density
				float alpha_noise = fastWorley3d(p * sources[i].params.w * 2.0 - time * 0.1);
				float density = clamp(heat * 2.0 - alpha_noise * 0.5, 0.0, 1.0);

				col = getFireColor(heat + noise * 0.05);
				// We pack albedo in rgb and density in a for later blending
				// But map() usually returns distance in .a, so we'll need to handle this in main.
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

	// Second pass: Subtraction of negative charges (Predators)
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

	float t = 0.0;
	vec4  res;
	bool  hit = false;

	for (int i = 0; i < 96; ++i) { // Iteration limit
		vec3 p = cameraPos + rayDir * t;
		res = map(p);
		if (res.a < 0.01) {
			hit = true;
			break;
		}
		t += res.a;
		if (t > sceneDistance || t > 1500.0)
			break;
	}

	if (hit) {
		vec3  p = cameraPos + rayDir * t;
		vec3  normal = getNormal(p);
		vec3  lightDir = normalize(vec3(0.5, 1.0, 0.5));
		float diff = max(dot(normal, lightDir), 0.0);

		// Recalculate density at the hit point for explosion types
		float final_alpha = 1.0;
		// We need a way to know if we hit an explosion source
		// For simplicity, let's re-map at the hit point and check parameters
		// Actually, we can just use the color returned by map() which we tweaked

		// Add a bit of rim light/glow
		float rim = 1.0 - max(dot(normal, -rayDir), 0.0);
		rim = pow(rim, 3.0);

		vec3 volumeColor = res.rgb * (diff * 0.8 + 0.2) + res.rgb * rim * 0.5;

		// Approximate alpha based on distance to surface and noise
		// In a real volume renderer we'd accumulate, but here we're raymarching to surface
		// Let's use Worley noise again to create "holes" or transparent regions
		float alpha_noise = fastWorley3d(p * 0.2 - time * 0.1);
		final_alpha = smoothstep(0.0, 0.2, 0.8 - alpha_noise);

		FragColor = vec4(mix(sceneColor, volumeColor, final_alpha), 1.0);
	} else {
		FragColor = vec4(sceneColor, 1.0);
	}
}
