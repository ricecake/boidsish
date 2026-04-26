#version 430 core

layout(location = 0) out vec4 FragColor;
in vec2  TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D historyTexture;
uniform vec2      u_sdfScreenSize;
uniform vec3      u_sdfCameraPos;
uniform mat4      u_sdfInvView;
uniform mat4      u_sdfInvProjection;
uniform float     u_sdfTime;

struct SdfSource {
	vec4 position_radius;        // xyz: pos, w: radius
	vec4 color_smoothness;       // rgb: color, a: smoothness
	vec4 high_sub_flags_charge;  // x: high_type, y: sub_type, z: flags, w: charge
	vec4 volumetric_params;      // x: density, y: absorption, z: noise_scale, w: noise_intensity
	vec4 color_inner_emission;   // rgb: inner color, a: emission intensity
	vec4 color_outer_ground;     // rgb: outer color, a: ground_y
    vec4 extra_params;           // Effect specific
    vec4 time_unused;            // x: normalized_time
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
#include "../helpers/lighting.glsl"

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

// High Types
#define SDF_HIGH_SOLID 0.0
#define SDF_HIGH_VOLUMETRIC 1.0
#define SDF_HIGH_ENVIRONMENTAL 2.0

// Flags
#define SDF_FLAG_REFRACTION 1
#define SDF_FLAG_REFLECTION 2

// --- Helpers ---

float smin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0);
    return min(a, b) - h * h * 0.25 / k;
}

// Exact distance to an ellipsoid
float sdEllipsoid(vec3 p, vec3 r) {
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / k1;
}

// Exact distance to a capped cylinder
float sdCappedCylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdMushroom(vec3 p, float maxRadius, float ntime) {
    float currentRadius = maxRadius * mix(0.1, 1.0, ntime);
    float stemHeight    = maxRadius * mix(0.0, 1.2, ntime);
    float capThickness  = currentRadius * mix(0.8, 0.3, ntime);

    vec3 capPos = p - vec3(0.0, stemHeight, 0.0);
    float cap = sdEllipsoid(capPos, vec3(currentRadius, capThickness, currentRadius));

    vec3 stemPos = p - vec3(0.0, stemHeight * 0.5, 0.0);
    float stem = sdCappedCylinder(stemPos, stemHeight * 0.5, currentRadius * 0.2);

    return smin(cap, stem, currentRadius * 0.3);
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

// --- Directive Evaluation ---

float sampleSolidSDF(vec3 p) {
    float d = 1e10;
    for (int i = 0; i < numSources; ++i) {
        if (sources[i].high_sub_flags_charge.x == SDF_HIGH_SOLID) {
            float d_src;
            if (sources[i].high_sub_flags_charge.y == 0.0) { // Sphere
                d_src = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
            } else {
                d_src = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
            }

            float charge = sources[i].high_sub_flags_charge.w;
            if (charge > 0.0) {
                d = smin(d, d_src, sources[i].color_smoothness.a);
            } else {
                float k = sources[i].color_smoothness.a;
                float h = clamp(0.5 - 0.5 * (d + d_src) / k, 0.0, 1.0);
                d = mix(d, -d_src, h) + k * h * (1.0 - h);
            }
        }
    }
    return d;
}

vec3 getSolidNormal(vec3 p) {
    vec2 e = vec2(0.01, 0.0);
    return normalize(vec3(
        sampleSolidSDF(p + e.xyy) - sampleSolidSDF(p - e.xyy),
        sampleSolidSDF(p + e.yxy) - sampleSolidSDF(p - e.yxy),
        sampleSolidSDF(p + e.yyx) - sampleSolidSDF(p - e.yyx)
    ));
}

vec3 getSolidColor(vec3 p) {
    float d_min = 1e10;
    vec3 col = vec3(1.0);
    for (int i = 0; i < numSources; ++i) {
        if (sources[i].high_sub_flags_charge.x == SDF_HIGH_SOLID) {
             float d_src = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
             if (d_src < d_min) {
                 d_min = d_src;
                 col = sources[i].color_smoothness.rgb;
             }
        }
    }
    return col;
}

struct SampledProperties {
    float density;
    float ior;
    vec3  emission;
    float absorption;
};

SampledProperties sampleDirectives(vec3 p) {
    SampledProperties res;
    res.density = 0.0;
    res.ior = 1.0;
    res.emission = vec3(0.0);
    res.absorption = 0.0;

    for (int i = 0; i < numSources; ++i) {
        float highType = sources[i].high_sub_flags_charge.x;
        float subType = sources[i].high_sub_flags_charge.y;
        int flags = int(sources[i].high_sub_flags_charge.z);
        float charge = sources[i].high_sub_flags_charge.w;
        vec3 center = sources[i].position_radius.xyz;
        float radius = sources[i].position_radius.w;
        float ntime = sources[i].time_unused.x;

        if (highType == SDF_HIGH_VOLUMETRIC) {
            float d;
            if (subType == 1.0) { // Mushroom/Explosion
                d = sdMushroom(p - center, radius, ntime);
            } else {
                d = sphereSDF(p - center, radius);
            }

            if (d < radius * 0.5) {
                float normalized_d = clamp(-d / radius, 0.0, 1.0);
                float noise = fastFbm3d(p * sources[i].volumetric_params.z + u_sdfTime * 0.2) * 0.5 + 0.5;
                float d_sample = normalized_d * noise * sources[i].volumetric_params.x;

                res.density += d_sample;
                res.absorption += d_sample * sources[i].volumetric_params.y;
                res.emission += d_sample * sources[i].color_inner_emission.rgb * sources[i].color_inner_emission.a;

                if ((flags & SDF_FLAG_REFRACTION) != 0) {
                    res.ior += d_sample * 0.2;
                }
            }
        } else if (highType == SDF_HIGH_ENVIRONMENTAL) {
            if (subType == 0.0) { // Wind
                vec3 wind = getWindAtPos(p);
                float windSpeed = length(wind);
                if (windSpeed > 10.0) {
                    float whisp = clamp((windSpeed - 10.0) / 30.0, 0.0, 1.0);
                    vec3 advectedP = p - wind * u_sdfTime * 0.1;
                    float noise = pow(fastFbm3d(advectedP * 0.5), 3.0);
                    res.density += whisp * noise * 0.5;
                    res.emission += sources[i].color_inner_emission.rgb * whisp * noise * 0.1;
                    if ((flags & SDF_FLAG_REFRACTION) != 0) {
                        res.ior += whisp * noise * 0.02;
                    }
                }
            } else if (subType == 1.0) { // Bubbles
                int gridIdx = getParticleGridHash(p);
                int head = grid_heads[gridIdx];
                int count = 0;
                while (head != -1 && count < 16) {
                    Particle part = particles[head];
                    if (part.style == STYLE_BUBBLES) {
                        float d = distance(p, part.pos.xyz) - 0.5;
                        if (d < 0.0) {
                            float edge = smoothstep(0.0, -0.2, d);
                            res.density += edge * 0.05;
                            if ((flags & SDF_FLAG_REFRACTION) != 0) {
                                res.ior += edge * 0.33;
                            }
                        }
                    }
                    head = grid_next[head];
                    count++;
                }
            }
        }
    }

    // Always apply weather heat lines as a baseline environmental if temperature is high
    vec4 scalars = getWeatherScalarsAtPos(p);
    float temp = scalars.x;
    if (temp > 300.0) {
        float heat = clamp((temp - 300.0) / 20.0, 0.0, 1.0);
        float noise = fastFbm3d(p * 2.0 + vec3(0.0, -u_sdfTime * 5.0, 0.0));
        res.ior += heat * noise * 0.05;
        res.density += heat * noise * 0.01;
    }

    return res;
}

vec3 calculateIORGradient(vec3 p) {
    vec2 e = vec2(0.1, 0.0);
    return vec3(
        sampleDirectives(p + e.xyy).ior - sampleDirectives(p - e.xyy).ior,
        sampleDirectives(p + e.yxy).ior - sampleDirectives(p - e.yxy).ior,
        sampleDirectives(p + e.yyx).ior - sampleDirectives(p - e.yyx).ior
    ) / (2.0 * e.x);
}

void refractiveVolumetricMarch(
	vec3 rayOrigin, vec3 rayDir, float maxDist,
	out vec3 accumColor, out float transmittance, out vec3 finalRayDir, out bool contributed, out bool hitSolid, out vec3 solidPos
) {
	accumColor = vec3(0.0);
	transmittance = 1.0;
    finalRayDir = rayDir;
    contributed = false;
    hitSolid = false;

    int steps = 48;
    float stepSize = maxDist / float(steps);
    float jitter = fastBlueNoise(TexCoords * u_sdfScreenSize * 0.0005 + u_sdfTime) * stepSize;

    vec3 p = rayOrigin + rayDir * jitter;
    vec3 currDir = rayDir;
    float t = jitter;

    for (int i = 0; i < steps; ++i) {
        if (t > maxDist) break;

        // Check for solid directives
        float d_solid = sampleSolidSDF(p);
        if (d_solid < 0.01) {
            hitSolid = true;
            solidPos = p;
            contributed = true;
            break;
        }

        SampledProperties prop = sampleDirectives(p);

        if (prop.density > 0.001 || abs(prop.ior - 1.0) > 0.001) {
            contributed = true;
            vec3 gradN = calculateIORGradient(p);
            currDir = normalize(currDir * prop.ior + gradN * stepSize);

            float alpha = 1.0 - exp(-prop.density * stepSize);
            accumColor += transmittance * alpha * prop.emission;
            transmittance *= exp(-(prop.absorption + prop.density) * stepSize);
        }

        p += currDir * stepSize;
        t += stepSize;
        if (transmittance < 0.01) break;
    }
    finalRayDir = currDir;
}

// --- Main ---

void main() {
	vec4 sceneColorSample = texture(sceneTexture, TexCoords);
	float depth = texture(depthTexture, TexCoords).r;

	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewTargetPos_ = u_sdfInvProjection * ndcPos;
	viewTargetPos_ /= viewTargetPos_.w;
	vec4 worldTargetPos = u_sdfInvView * viewTargetPos_;
	float sceneDistance = length(worldTargetPos.xyz - u_sdfCameraPos);
	if (depth >= 0.999999) sceneDistance = 1000.0;

	vec4 target = u_sdfInvProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((u_sdfInvView * vec4(normalize(target.xyz), 0.0)).xyz);

	vec3  volAccumColor;
	float transmittance;
    vec3  finalRayDir;
    bool  contributed;
    bool  hitSolid;
    vec3  solidPos;
	refractiveVolumetricMarch(u_sdfCameraPos, rayDir, sceneDistance, volAccumColor, transmittance, finalRayDir, contributed, hitSolid, solidPos);

    if (!contributed) {
        FragColor = sceneColorSample;
        return;
    }

    vec3 baseColor;
    if (hitSolid) {
        vec3 normal = getSolidNormal(solidPos);
        vec3 albedo = getSolidColor(solidPos);
        float primaryShadow;
        baseColor = apply_lighting_pbr_no_shadows(solidPos, normal, albedo, 0.5, 0.0, 1.0, primaryShadow).rgb;
    } else {
        vec3 refractedWorldPos = u_sdfCameraPos + finalRayDir * sceneDistance;
        vec4 refractedClip = viewProjection * vec4(refractedWorldPos, 1.0);
        vec2 refractedUV = (refractedClip.xy / refractedClip.w) * 0.5 + 0.5;

        if (refractedUV.x < 0.0 || refractedUV.x > 1.0 || refractedUV.y < 0.0 || refractedUV.y > 1.0) {
            refractedUV = TexCoords;
        }
        baseColor = texture(sceneTexture, refractedUV).rgb;
    }

    vec3 currentFrameColor = volAccumColor + baseColor * transmittance;

    vec4 prevClip = prevViewProjection * vec4(worldTargetPos.xyz, 1.0);
    vec2 prevUV = (prevClip.xy / prevClip.w) * 0.5 + 0.5;

    if (prevUV.x >= 0.0 && prevUV.x <= 1.0 && prevUV.y >= 0.0 && prevUV.y <= 1.0 && frameIndex > 0) {
        vec4 historyColor = texture(historyTexture, prevUV);
        FragColor = mix(vec4(currentFrameColor, 1.0), historyColor, 0.8);
    } else {
        FragColor = vec4(currentFrameColor, 1.0);
    }
}
