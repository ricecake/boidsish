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
	vec4 position_radius;      // xyz: pos, w: radius
	vec4 color_smoothness;     // rgb: color, a: smoothness
	vec4 charge_type_vol_unused; // x: charge, y: type, z: volumetric, w: unused
	vec4 volumetric_params;    // x: density, y: absorption, z: noise_scale, w: noise_intensity
	vec4 color_inner;          // rgb: inner color, a: unused
	vec4 color_outer;          // rgb: outer color, a: unused
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
		if (sources[i].charge_type_vol_unused.x > 0.0 && sources[i].charge_type_vol_unused.z < 0.5) {
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
		if (sources[i].charge_type_vol_unused.x < 0.0) {
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
        if (sources[i].charge_type_vol_unused.x > 0.0 && sources[i].charge_type_vol_unused.z < 0.5) {
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
        if (sources[i].charge_type_vol_unused.x < 0.0) {
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

// Volumetric density function
float getVolumetricDensity(vec3 p, int index) {
    vec3 center = sources[index].position_radius.xyz;
    float radius = sources[index].position_radius.w;
    float d = sphereSDF(p - center, radius);

    if (d > 0.0) return 0.0;

    // Density increases towards the center
    float normalized_d = -d / radius; // 0 at surface, 1 at center
    float density = sources[index].volumetric_params.x * normalized_d;

    // Add noise
    float noise_scale = sources[index].volumetric_params.z;
    float noise_intensity = sources[index].volumetric_params.w;
    float n = snoise3d(p * noise_scale + time * 0.5);
    density *= (1.0 + n * noise_intensity);

    return max(0.0, density);
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

    // --- Part 2: Volumetric Raymarching ---
    vec3 volAccumColor = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < numSources; ++i) {
        if (sources[i].charge_type_vol_unused.z > 0.5) { // Volumetric
            vec3 center = sources[i].position_radius.xyz;
            float radius = sources[i].position_radius.w;

            // Check intersection with bounding sphere
            vec3 co = cameraPos - center;
            float b_dot = dot(rayDir, co);
            float c_det = dot(co, co) - radius * radius;
            float det = b_dot * b_dot - c_det;

            if (det > 0.0) {
                float t1 = -b_dot - sqrt(det);
                float t2 = -b_dot + sqrt(det);

                float t_start = max(0.0, t1);
                float t_end = min(sceneDistance, t2);

                if (t_start < t_end) {
                    // Raymarch through the volume
                    float stepSize = (t_end - t_start) / 32.0;
                    for (int j = 0; j < 32; ++j) {
                        float curT = t_start + stepSize * (float(j) + 0.5);
                        vec3 p = cameraPos + rayDir * curT;

                        float d = sphereSDF(p - center, radius);
                        if (d < 0.0) {
                            float density = getVolumetricDensity(p, i);
                            float absorption = sources[i].volumetric_params.y;

                            float normalized_d = -d / radius;
                            vec3 color = mix(sources[i].color_outer.rgb, sources[i].color_inner.rgb, normalized_d);

                            float alpha = 1.0 - exp(-density * stepSize);
                            volAccumColor += transmittance * alpha * color;
                            transmittance *= exp(-absorption * density * stepSize);

                            if (transmittance < 0.01) break;
                        }
                    }
                }
            }
        }
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

    // --- Part 3: Temporal Reprojection ---
    vec3 reprojWorldPos = worldPos.xyz;
    if (hit_surface && t_surface < sceneDistance) {
        reprojWorldPos = cameraPos + rayDir * t_surface;
    }
    vec4 reprojectedPos = prevViewProjection * vec4(reprojWorldPos, 1.0);
    vec2 prevTexCoords = (reprojectedPos.xy / reprojectedPos.w) * 0.5 + 0.5;

    vec4 historyColor = texture(historyTexture, prevTexCoords);

    // Check if reprojected coordinates are within screen bounds
    bool onScreen = prevTexCoords.x >= 0.0 && prevTexCoords.x <= 1.0 &&
                    prevTexCoords.y >= 0.0 && prevTexCoords.y <= 1.0;

    float blendFactor = (onScreen && frameIndex > 0) ? 0.9 : 0.0;

    FragColor = mix(vec4(currentFrameColor, 1.0), historyColor, blendFactor);
}
