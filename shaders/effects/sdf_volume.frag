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

// Custom union that handles color blending
vec4 opUnionColored(vec4 d1, vec4 d2, float k) {
	float h = clamp(0.5 + 0.5 * (d2.a - d1.a) / k, 0.0, 1.0);
	float res_d = mix(d2.a, d1.a, h) - k * h * (1.0 - h);
	vec3  res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

// Custom subtraction that handles color
vec4 opSubtractionColored(vec4 d1, vec4 d2, float k) {
	float h = clamp(0.5 - 0.5 * (d2.a + d1.a) / k, 0.0, 1.0);
	float res_d = mix(d2.a, -d1.a, h) + k * h * (1.0 - h);
	vec3 res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

// Map function for surfaces (used in sphere tracing)
float mapDistance(vec3 p) {
	float d = 1e10;

	// Union of positive charges
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

	// Subtraction of negative charges
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
            if (first) {
                res = vec4(sources[i].color_smoothness.rgb, d);
                first = false;
            } else {
                res = opUnionColored(vec4(sources[i].color_smoothness.rgb, d), res, sources[i].color_smoothness.a);
            }
        }
    }

    for (int i = 0; i < numSources; ++i) {
        if (sources[i].charge_type_vol_time.x < 0.0) {
            float d = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
            if (!first) {
                res = opSubtractionColored(vec4(sources[i].color_smoothness.rgb, d), res, sources[i].color_smoothness.a);
            }
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

// Multi-octave turbulent noise for billowing explosion shapes
float turbulentNoise(vec3 p, float scale, int octaves) {
    float val = 0.0;
    float amp = 1.0;
    float freq = scale;
    float total_amp = 0.0;
    for (int i = 0; i < octaves; i++) {
        val += abs(snoise3d(p * freq)) * amp;
        total_amp += amp;
        freq *= 2.1;
        amp *= 0.45;
    }
    return val / total_amp;
}

// Evaluate the mushroom-shaped distance field.
// Warps coordinate space so the silhouette itself is non-spherical:
// vertically elongated, with a wide cap and narrow stem.
float mushroomSDF(vec3 rel, float radius, float ntime) {
    // Vertical elongation increases with time — early fireball is rounder,
    // mature explosion is taller. This also means higher-intensity explosions
    // (which have larger radius and longer lifetime) get more pronounced mushrooms.
    float elongation = mix(1.2, 1.8, ntime);
    vec3 warped = rel;
    warped.y /= elongation;

    // Height-dependent horizontal pinch: narrow at the bottom (stem),
    // wide at the top (cap). This is the key to the mushroom silhouette.
    float height_frac = clamp((warped.y / radius) + 0.5, 0.0, 1.0);
    // Stem is pinched to ~40% width, cap expands to ~120%
    float xz_scale = mix(0.4, 1.2, smoothstep(0.15, 0.6, height_frac));
    warped.xz /= xz_scale;

    return sphereSDF(warped, radius);
}

// Volumetric density for explosions with mushroom cloud and roiling
float getVolumetricDensity(vec3 p, int index) {
    vec3 center = sources[index].position_radius.xyz;
    float radius = sources[index].position_radius.w;
    float ground_y = sources[index].color_outer.a;
    float ntime = sources[index].charge_type_vol_time.w;

    if (p.y < ground_y) return 0.0;

    vec3 rel = p - center;

    // Use the mushroom-shaped SDF instead of a plain sphere
    float d = mushroomSDF(rel, radius, ntime);

    // Allow some density beyond the hard surface for soft edges
    if (d > radius * 0.05) return 0.0;

    float normalized_d = clamp(-d / radius, 0.0, 1.0);

    // Height profile within the mushroom: dense cap, thinner stem
    float height_frac = clamp((rel.y / radius) + 0.5, 0.0, 1.0);
    float cap_density = smoothstep(0.0, 0.25, height_frac) * (0.3 + 0.7 * smoothstep(0.35, 0.75, height_frac));

    float density = sources[index].volumetric_params.x * normalized_d * cap_density;

    // Deep roiling turbulence
    float noise_scale = sources[index].volumetric_params.z;
    float noise_intensity = sources[index].volumetric_params.w;

    // Noise drifts upward to simulate rising hot gas
    vec3 noise_pos = p * noise_scale + vec3(0.0, -time * 1.5, 0.0);
    float turb = turbulentNoise(noise_pos, 1.0, 4);

    // Large-scale billowing structure
    float billow = snoise3d(noise_pos * 0.4 + time * 0.3) * 0.5 + 0.5;

    density *= mix(0.2, 2.0, turb) * mix(0.5, 1.3, billow);

    // Soft edges
    float edge_fade = smoothstep(0.0, 0.12, normalized_d);
    density *= edge_fade;

    // Ground interaction: rolling dense base
    float ground_dist = (p.y - ground_y) / max(radius, 0.01);
    if (ground_dist < 0.3) {
        density *= 1.0 + 2.5 * smoothstep(0.3, 0.0, ground_dist);
    }

    return max(0.0, density);
}

// Explosion color based on temperature (normalized_d) and time
vec3 explosionColor(float normalized_d, float ntime, vec3 color_inner, vec3 color_outer) {
    // Temperature gradient: hot core → warm middle → cool smoke
    // As time progresses, everything cools (shifts toward smoke)
    float temperature = normalized_d * (1.0 - ntime * 0.7);

    // White-hot core → yellow → orange → red → dark smoke
    vec3 white_hot = vec3(1.0, 0.95, 0.8);
    vec3 yellow    = vec3(1.0, 0.8, 0.2);
    vec3 orange    = color_inner;
    vec3 red       = color_outer;
    vec3 smoke     = vec3(0.15, 0.1, 0.08);

    vec3 col;
    if (temperature > 0.8)
        col = mix(orange, white_hot, (temperature - 0.8) / 0.2);
    else if (temperature > 0.5)
        col = mix(yellow, orange, (temperature - 0.5) / 0.3);
    else if (temperature > 0.25)
        col = mix(red, yellow, (temperature - 0.25) / 0.25);
    else
        col = mix(smoke, red, temperature / 0.25);

    return col;
}

void main() {
	vec4 sceneColorSample = texture(sceneTexture, TexCoords);
    vec3 sceneColor = sceneColorSample.rgb;
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
	bool  hit_surface = false;

    // --- Part 1: Sphere Tracing for Surfaces ---
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

    vec3 currentFrameColor = sceneColor;
    float t_surface = t;
    bool had_sdf_contribution = hit_surface && t_surface < sceneDistance;

    // --- Part 2: Volumetric Raymarching ---
    vec3 volAccumColor = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < numSources; ++i) {
        if (sources[i].charge_type_vol_time.z > 0.5) { // Volumetric
            vec3 center = sources[i].position_radius.xyz;
            float radius = sources[i].position_radius.w;
            float ntime = sources[i].charge_type_vol_time.w;
            float emission = sources[i].color_inner.a;

            // Expand bounding sphere to cover the elongated mushroom shape
            // (up to 1.8x vertical stretch + noise displacement)
            float bound_radius = radius * 2.0;

            // Check intersection with bounding sphere
            vec3 co = cameraPos - center;
            float b_dot = dot(rayDir, co);
            float c_det = dot(co, co) - bound_radius * bound_radius;
            float det = b_dot * b_dot - c_det;

            if (det > 0.0) {
                float t1 = -b_dot - sqrt(det);
                float t2 = -b_dot + sqrt(det);

                float t_start = max(0.0, t1);
                float t_end = min(sceneDistance, t2);

                if (t_start < t_end) {
                    // More samples for quality; adaptive based on radius
                    int num_steps = 48;
                    float stepSize = (t_end - t_start) / float(num_steps);

                    for (int j = 0; j < num_steps; ++j) {
                        float curT = t_start + stepSize * (float(j) + 0.5);
                        vec3 p = cameraPos + rayDir * curT;

                        float density = getVolumetricDensity(p, i);
                        if (density <= 0.0) continue;

                        float absorption = sources[i].volumetric_params.y;
                        float d = mushroomSDF(p - center, radius, ntime);
                        float normalized_d = clamp(-d / radius, 0.0, 1.0);

                        // Temperature-based color with time evolution
                        vec3 color = explosionColor(
                            normalized_d, ntime,
                            sources[i].color_inner.rgb,
                            sources[i].color_outer.rgb
                        );

                        // Self-illumination: hot core glows, fading over time
                        float emit = emission * normalized_d * (1.0 - ntime);
                        color += color * emit;

                        float alpha = 1.0 - exp(-density * stepSize);
                        volAccumColor += transmittance * alpha * color;
                        transmittance *= exp(-absorption * density * stepSize);

                        if (transmittance < 0.01) break;
                    }
                }
            }
        }
    }

    // Mark as SDF-affected if we accumulated any volumetric content
    if (transmittance < 0.99) {
        had_sdf_contribution = true;
    }

	if (hit_surface && t_surface < sceneDistance) {
		vec3  p = cameraPos + rayDir * t_surface;
		vec3  normal = getNormal(p);
		vec3  lightDir = normalize(vec3(0.5, 1.0, 0.5));
		float diff = max(dot(normal, lightDir), 0.0);

		float rim = 1.0 - max(dot(normal, -rayDir), 0.0);
		rim = pow(rim, 3.0);

        res = mapColor(p);
		vec3 surfaceColor = res.rgb * (diff * 0.8 + 0.2) + res.rgb * rim * 0.5;
        currentFrameColor = volAccumColor + transmittance * surfaceColor;
	} else {
		currentFrameColor = volAccumColor + transmittance * sceneColor;
	}

    // --- Part 3: Temporal Reprojection (only for SDF-affected pixels) ---
    if (!had_sdf_contribution) {
        // No SDF content here — pass scene through directly, no history blend
        FragColor = vec4(currentFrameColor, 1.0);
    } else {
        // For surface hits use the surface point; for volumetric-only use scene depth
        vec3 reprojWorldPos = (hit_surface && t_surface < sceneDistance)
            ? cameraPos + rayDir * t_surface
            : worldPos.xyz;
        vec4 reprojectedPos = prevViewProjection * vec4(reprojWorldPos, 1.0);
        vec2 prevTexCoords = (reprojectedPos.xy / reprojectedPos.w) * 0.5 + 0.5;

        vec4 historyColor = texture(historyTexture, prevTexCoords);

        bool onScreen = prevTexCoords.x >= 0.0 && prevTexCoords.x <= 1.0 &&
                        prevTexCoords.y >= 0.0 && prevTexCoords.y <= 1.0;

        // Lower blend for volumetric content so turbulence detail isn't washed out
        float baseBlend = (hit_surface && t_surface < sceneDistance) ? 0.85 : 0.6;
        float blendFactor = (onScreen && frameIndex > 0) ? baseBlend : 0.0;

        FragColor = mix(vec4(currentFrameColor, 1.0), historyColor, blendFactor);
    }
}
