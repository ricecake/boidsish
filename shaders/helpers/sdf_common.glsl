#ifndef HELPERS_SDF_COMMON_GLSL
#define HELPERS_SDF_COMMON_GLSL

#include "lygia/sdf/sphereSDF.glsl"

struct SdfSource {
	vec4 position_radius;        // xyz: pos, w: radius
	vec4 color_smoothness;       // rgb: color, a: smoothness
	vec4 charge_type_vol_time;   // x: charge, y: type, z: volumetric, w: normalized_time (0-1)
	vec4 volumetric_params;      // x: density, y: absorption, z: noise_scale, w: noise_intensity
	vec4 color_inner;            // rgb: inner color, a: emission intensity
	vec4 color_outer;            // rgb: outer color, a: ground_y
};

#ifndef SDF_VOLUMES_BUFFER_DEFINED
#define SDF_VOLUMES_BUFFER_DEFINED
layout(std430, binding = [[SDF_VOLUMES_BINDING]]) buffer SdfVolumes {
	int       numSources;
	SdfSource sources[];
};
#endif

float smin( float a, float b, float k )
{
    float h = max(k-abs(a-b),0.0);
    return min(a, b) - h*h*0.25/k;
}

float mushroomSDF(vec3 p, float radius, float ntime) {
    p.y -= radius;

    // 1. Normalize the height to a 0.0 (base) to 1.0 (top) range
    float h = clamp((p.y + radius) / (2.0 * radius), 0.0, 1.0);

    // 2. Define the two flares independently
    // Cap flare: 0.0 at the stem, smoothly reaching 1.0 at the top
    float cap_flare = smoothstep(0.4, 0.9, h)*1.43;

    // Base flare: 1.0 at the very bottom, smoothly dropping to 0.0 at the stem
    float base_flare = smoothstep(0.3, 0.0, h);

    // 3. Scale the base dynamically based on time so it keeps expanding
    // You can adjust the 2.5 multiplier to control how wide the base gets
    float base_width_over_time = mix(0.0, 2.5, ntime);

    // 4. Combine to form the final profile
    // 0.2 is your core stem thickness
    float target_pinch = 0.2 + cap_flare + (base_flare * base_width_over_time);

    // 5. Morph from a sphere (pinch = 1.0) to the mushroom profile
    float pinch = mix(1.0, target_pinch, smoothstep(0.0, 0.6, ntime));

    vec3 warped = p;
    warped.xz /= pinch;

    return ((length(warped) - radius) * 0.4);
}

float getSdfDistance(vec3 p, int i) {
    float type = sources[i].charge_type_vol_time.y;
    float ntime = sources[i].charge_type_vol_time.w;
    vec3 center = sources[i].position_radius.xyz;
    float radius = sources[i].position_radius.w;

    if (type > 0.5) { // Mushroom
        return mushroomSDF(p - center, radius, ntime);
    } else { // Sphere
        return sphereSDF(p - center, radius);
    }
}

float mapDistance(vec3 p) {
	float d = 1e10;
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x > 0.0 && sources[i].charge_type_vol_time.z < 0.5) {
			float d_src = getSdfDistance(p, i);
			if (d > 1e9) d = d_src;
			else {
				float k = sources[i].color_smoothness.a;
				d = smin(d, d_src, k);
			}
		}
	}
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x < 0.0) {
			float d_src = getSdfDistance(p, i);
			float k = sources[i].color_smoothness.a;
			float h = clamp(0.5 - 0.5 * (d + d_src) / k, 0.0, 1.0);
			d = mix(d, -d_src, h) + k * h * (1.0 - h);
		}
	}
	return d;
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

vec4 mapColor(vec3 p) {
	vec4 res = vec4(1.0, 1.0, 1.0, 1e10);
	bool first = true;
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x > 0.0 && sources[i].charge_type_vol_time.z < 0.5) {
			float d = getSdfDistance(p, i);
			if (first) { res = vec4(sources[i].color_smoothness.rgb, d); first = false; }
			else { res = opUnionColored(vec4(sources[i].color_smoothness.rgb, d), res, sources[i].color_smoothness.a); }
		}
	}
	for (int i = 0; i < numSources; ++i) {
		if (sources[i].charge_type_vol_time.x < 0.0) {
			float d = getSdfDistance(p, i);
			if (!first) { res = opSubtractionColored(vec4(sources[i].color_smoothness.rgb, d), res, sources[i].color_smoothness.a); }
		}
	}
	return res;
}

#endif // HELPERS_SDF_COMMON_GLSL
