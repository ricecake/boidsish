#ifndef RESTIR_COMMON_GLSL
#define RESTIR_COMMON_GLSL

#include "../types/lighting.glsl"
#include "../helpers/constants.glsl"
#include "../helpers/brdf.glsl"
#include "particle_types.glsl"

struct Reservoir {
    uint  light_index; // Index into AllLights SSBO or fire particles
    float w_sum;       // Sum of weights
    float m;           // Number of samples seen
    float W;           // Normalization factor
};

struct LightData {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    int type;
};

layout(std430, binding = [[ALL_LIGHTS_BINDING]]) buffer AllLights {
    Light gpu_lights[];
};

// // Fire particle access
// struct Particle {
// 	vec4 pos;
// 	vec4 vel;
// 	vec3 epicenter;
// 	int   style;
// 	int   emitter_index;
// 	int   emitter_id;
// 	float extras[2];
// };

// layout(std430, binding = [[PARTICLE_BUFFER_BINDING]]) buffer ParticleBuffer {
//     Particle particles[];
// };

#ifndef RESTIR_UNIFORMS_DEFINED
#define RESTIR_UNIFORMS_DEFINED
uniform int u_num_lights;
uniform int u_num_fire_particles;
uniform mat4 view;

#include "../types/temporal_data.glsl"
#endif

LightData getLightData(uint index) {
    LightData ld;
    if (index < uint(u_num_lights)) {
        ld.position = gpu_lights[index].position;
        ld.direction = gpu_lights[index].direction;
        ld.color = gpu_lights[index].color;
        ld.intensity = gpu_lights[index].intensity;
        ld.type = gpu_lights[index].type;
        // Directional light intensity boost for importance sampling
        if (ld.type == 1) ld.intensity *= 100.0;
    } else {
        uint p_idx = index - uint(u_num_lights);
        Particle p = particles[p_idx];
        if (p.style == STYLE_FIRE) {
            ld.position = particles[p_idx].pos.xyz;
            ld.direction = vec3(0, -1, 0);
            // Fire particles are typically orange/yellow/white
            ld.color = vec3(1.0, 0.6, 0.2);
            // Use lifetime (w) as intensity base for fire particles
            ld.intensity = clamp(particles[p_idx].pos.w, 0.0, 1.0);
            ld.type = 0; // POINT
        } else if (p.style == STYLE_AMBIENT && p.emitter_id == 4 ) {
            ld.position = particles[p_idx].pos.xyz;
            ld.direction = vec3(0, -1, 0);
            // Fire particles are typically orange/yellow/white
            ld.color = vec3(0.7, 0.9, 0.1);
            // Use lifetime (w) as intensity base for fire particles
            ld.intensity = clamp(particles[p_idx].pos.w, 0.0, 1.0);
            ld.type = 0; // POINT
        } else {
            ld.position = particles[p_idx].pos.xyz;
            ld.direction = vec3(0, -1, 0);
            // Fire particles are typically orange/yellow/white
            ld.color = vec3(0.2, 0.5, 0.1);
            // Use lifetime (w) as intensity base for fire particles
            ld.intensity = 0.0;//clamp(particles[p_idx].pos.w, 0.0, 1.0) * 5.0;
            ld.type = 0; // POINT
        }
    }
    return ld;
}

float safeDiv(float a, float b) {
    return (b != 0.0) ? (a / b) : 0.0;
}

float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

uint hash(uint x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return x;
}

float targetFunction(vec3 pos, vec3 norm, vec3 V, vec3 albedo, float roughness, float metallic, LightData ld) {
    vec3 L;
    float attenuation = 1.0;

    if (ld.type == 1) { // DIRECTIONAL
        L = normalize(-ld.direction);
        attenuation = 1.0;
    } else {
        vec3 L_vec = ld.position - pos;
        float distSq = dot(L_vec, L_vec);
        L = normalize(L_vec);
        // Using a slightly more robust falloff for ReSTIR importance
        attenuation = 1.0 / max(0.0001, distSq);
    }

    float dotNL = max(0.0, dot(norm, L));
    if (dotNL <= 0.0) return 0.0;

    vec3 H = normalize(V + L);
    float dotNV = max(1e-4, dot(norm, V));
    float dotNH = max(0.0, dot(norm, H));
    float dotHV = max(0.0, dot(H, V));

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlickFast(dotHV, F0);
    float D = DistributionGGX(norm, H, roughness);
    float Vis = VisibilitySmithGGXCorrelated(dotNL, dotNV, roughness);
    vec3 specular = D * Vis * F;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 brdf = (kD * albedo / PI) + specular;
    vec3 radiance = ld.color * ld.intensity * attenuation;

    vec3 final_color = brdf * radiance * dotNL;

    float lum = luminance(final_color);
    if (isnan(lum) || isinf(lum)) return 0.0;
    return max(0.0, lum);
}

void updateReservoir(inout Reservoir r, uint light_index, float weight, float random) {
    if (isnan(weight) || isinf(weight) || weight <= 0.0) return;
    r.w_sum += weight;
    r.m += 1.0;
    if (random * r.w_sum < weight) {
        r.light_index = light_index;
    }
    // Clamp weight sum to prevent numerical explosion
    r.w_sum = min(r.w_sum, 1e7);
}

// Simple screen-space visibility check against depth buffer
bool checkVisibility(vec3 viewPos, vec3 targetViewPos, sampler2D depthTex) {
    vec3 rayDir = targetViewPos - viewPos;
    float dist = length(rayDir);
    if (dist < 0.01) return true;
    rayDir /= dist;

    const int steps = 8;
    float stepSize = dist / float(steps);
    // Add bias to start position to prevent self-occlusion
    vec3 currentPos = viewPos + rayDir * (stepSize + 0.05 * worldScale);

    for (int i = 0; i < steps - 1; i++) {
        vec4 clip = uProjection * vec4(currentPos, 1.0);
        vec3 ndc = clip.xyz / clip.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;

        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return true;

        float d = textureLod(depthTex, uv, 0.0).r;
        vec4 sampledClip = vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
        vec4 sampledView = invProjection * sampledClip;
        float sampledDepth = sampledView.z / sampledView.w;

        // If current ray point is significantly behind the sampled depth, it's occluded
        // We use a slightly more generous bias here for robustness
        if (currentPos.z < sampledDepth - 0.2 * worldScale) return false;

        currentPos += rayDir * stepSize;
    }

    return true;
}

#endif
