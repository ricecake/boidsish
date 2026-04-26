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

layout(std430, binding = [[SDF_VOLUMES_BINDING]]) buffer SdfVolumes {
	int       numSources;
	SdfSource sources[];
};

#include "lygia/sdf/sphereSDF.glsl"
#include "lygia/lighting/blackbody.glsl"
#include "../helpers/fast_noise.glsl"
#include "../helpers/noise.glsl"
#include "../particle_types.glsl"

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

layout(binding = [[WIND_TEXTURE_BINDING]]) uniform sampler2D u_windTexture;
layout(binding = [[WEATHER_SCALARS_TEXTURE_BINDING]]) uniform sampler2D u_weatherScalars;

layout(std140, binding = [[WIND_DATA_BINDING]]) uniform WindData {
	ivec4 u_windOriginSize; // x, z = origin in chunks, y = size (width), w = height (60)
	vec4  u_windParams;     // x = chunkSpacing (32.0), y = time, z = curlScale, w = curlStrength
};

// --- Helpers ---

float smin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0);
    return min(a, b) - h * h * 0.25 / k;
}

// Environment lookups
vec3 getWindAtPos(vec3 worldPos) {
    if (u_windOriginSize.y <= 0) return vec3(0.0);
    float gridSpacing = u_windParams.x;
    vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);
    vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);
    return texture(u_windTexture, uv).xyz;
}

vec4 getWeatherScalarsAtPos(vec3 worldPos) {
    if (u_windOriginSize.y <= 0) return vec4(288.0, 0.5, 1013.0, 0.0);
    float gridSpacing = u_windParams.x;
    vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);
    vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);
    return texture(u_weatherScalars, uv);
}

// Particle lookup
int getParticleGridHash(vec3 pos) {
    ivec3 ipos = ivec3(floor(pos / [[PARTICLE_GRID_CELL_SIZE]]));
    uint h = uint(ipos.x * 73856093) ^ uint(ipos.y * 19349663) ^ uint(ipos.z * 83492791);
    return int(h % [[PARTICLE_GRID_SIZE]]);
}

// --- Volumetric Refraction Logic ---

struct SampledProperties {
    float density;
    float ior;
    vec3  emission;
    float absorption;
};

SampledProperties sampleVolume(vec3 p) {
    SampledProperties res;
    res.density = 0.0;
    res.ior = 1.0;
    res.emission = vec3(0.0);
    res.absorption = 0.0;

    // 1. Base SDF Volumes
    for (int i = 0; i < numSources; ++i) {
        if (sources[i].charge_type_vol_time.z < 0.5) continue;

        vec3  center = sources[i].position_radius.xyz;
        float radius = sources[i].position_radius.w;
        float d = sphereSDF(p - center, radius);

        if (d < radius * 0.5) {
            float normalized_d = clamp(-d / radius, 0.0, 1.0);
            float noise = fastFbm3d(p * sources[i].volumetric_params.z + time * 0.2) * 0.5 + 0.5;
            float d_sample = normalized_d * noise * sources[i].volumetric_params.x;

            res.density += d_sample;
            res.absorption += d_sample * sources[i].volumetric_params.y;
            res.emission += d_sample * sources[i].color_inner.rgb * sources[i].color_inner.a;
            // Density increases IOR
            res.ior += d_sample * 0.2;
        }
    }

    // 2. Weather Heat Lines
    vec4 scalars = getWeatherScalarsAtPos(p);
    float temp = scalars.x; // Kelvin
    if (temp > 300.0) {
        float heat = clamp((temp - 300.0) / 20.0, 0.0, 1.0);
        float noise = fastFbm3d(p * 2.0 + vec3(0.0, -time * 5.0, 0.0));
        res.ior += heat * noise * 0.05;
    }

    // 3. Wind Whisps
    vec3 wind = getWindAtPos(p);
    float windSpeed = length(wind);
    if (windSpeed > 10.0) {
        float whisp = clamp((windSpeed - 10.0) / 30.0, 0.0, 1.0);
        vec3 advectedP = p - wind * time * 0.1;
        float noise = pow(fastFbm3d(advectedP * 0.5), 3.0);
        res.density += whisp * noise * 0.5;
        res.ior += whisp * noise * 0.02;
        res.emission += vec3(0.8, 0.9, 1.0) * whisp * noise * 0.1;
    }

    // 4. Particle Bubbles
    int gridIdx = getParticleGridHash(p);
    int head = grid_heads[gridIdx];
    int count = 0;
    while (head != -1 && count < 8) {
        Particle part = particles[head];
        if (part.style == STYLE_BUBBLES) {
            float d = distance(p, part.pos.xyz) - 0.5;
            if (d < 0.0) {
                float edge = smoothstep(0.0, -0.2, d);
                res.ior += edge * 0.33;
                res.density += edge * 0.05;
            }
        }
        head = grid_next[head];
        count++;
    }

    return res;
}

vec3 calculateIORGradient(vec3 p) {
    vec2 e = vec2(0.1, 0.0);
    return vec3(
        sampleVolume(p + e.xyy).ior - sampleVolume(p - e.xyy).ior,
        sampleVolume(p + e.yxy).ior - sampleVolume(p - e.yxy).ior,
        sampleVolume(p + e.yyx).ior - sampleVolume(p - e.yyx).ior
    ) / (2.0 * e.x);
}

void refractiveVolumetricMarch(
	vec3 rayOrigin, vec3 rayDir, float maxDist,
	out vec3 accumColor, out float transmittance, out vec3 finalRayDir
) {
	accumColor = vec3(0.0);
	transmittance = 1.0;
    finalRayDir = rayDir;

    int steps = 64;
    float stepSize = maxDist / float(steps);
    float jitter = fastBlueNoise(TexCoords * screenSize * 0.0005 + time) * stepSize;

    vec3 p = rayOrigin + rayDir * jitter;
    vec3 currDir = rayDir;

    for (int i = 0; i < steps; ++i) {
        SampledProperties prop = sampleVolume(p);

        if (prop.density > 0.001 || abs(prop.ior - 1.0) > 0.001) {
            // Update direction based on IOR gradient
            vec3 gradN = calculateIORGradient(p);
            currDir = normalize(currDir * prop.ior + gradN * stepSize);

            float alpha = 1.0 - exp(-prop.density * stepSize);
            accumColor += transmittance * alpha * prop.emission;
            transmittance *= exp(-(prop.absorption + prop.density) * stepSize);
        }

        p += currDir * stepSize;
        if (transmittance < 0.01) break;
    }
    finalRayDir = currDir;
}

// --- Main ---

void main() {
	vec4 sceneColorSample = texture(sceneTexture, TexCoords);
	float depth = texture(depthTexture, TexCoords).r;

	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPos = invProjection * ndcPos;
	viewPos /= viewPos.w;
	vec4 worldPos = invView * viewPos;
	float sceneDistance = length(worldPos.xyz - cameraPos);
	if (depth >= 0.999999) sceneDistance = 1000.0;

	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

	// Unified Volumetric Refractive March
	vec3  volAccumColor;
	float transmittance;
    vec3  finalRayDir;
	refractiveVolumetricMarch(cameraPos, rayDir, sceneDistance, volAccumColor, transmittance, finalRayDir);

    // Reproject background UV based on final ray direction
    vec3 refractedWorldPos = cameraPos + finalRayDir * sceneDistance;
    vec4 refractedClip = viewProjection * vec4(refractedWorldPos, 1.0);
    vec2 refractedUV = (refractedClip.xy / refractedClip.w) * 0.5 + 0.5;

    // Boundary check for UV
    if (refractedUV.x < 0.0 || refractedUV.x > 1.0 || refractedUV.y < 0.0 || refractedUV.y > 1.0) {
        refractedUV = TexCoords;
    }

    vec3 sceneColor = texture(sceneTexture, refractedUV).rgb;
    FragColor = vec4(volAccumColor + sceneColor * transmittance, 1.0);
}
