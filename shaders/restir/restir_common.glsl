#ifndef RESTIR_COMMON_GLSL
#define RESTIR_COMMON_GLSL

#include "../types/lighting.glsl"
#include "../helpers/constants.glsl"

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

// Fire particle access
struct Particle {
	vec4 pos;
	vec4 vel;
	vec3 epicenter;
	int   style;
	int   emitter_index;
	int   emitter_id;
	float extras[2];
};

layout(std430, binding = [[PARTICLE_BUFFER_BINDING]]) buffer ParticleBuffer {
    Particle particles[];
};

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
        ld.position = particles[p_idx].pos.xyz;
        ld.direction = vec3(0, -1, 0);
        // Fire particles are typically orange/yellow/white
        ld.color = vec3(1.0, 0.6, 0.2);
        // Use lifetime (w) as intensity base for fire particles
        ld.intensity = clamp(particles[p_idx].pos.w, 0.0, 1.0) * 5.0;
        ld.type = 0; // POINT
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

float targetFunction(vec3 pos, vec3 norm, LightData ld) {
    vec3 L;
    float attenuation = 1.0;

    if (ld.type == 1) { // DIRECTIONAL
        L = normalize(-ld.direction);
        // For directional lights, we use a constant large distance for p_hat calculation
        attenuation = 100.0;
    } else {
        vec3 L_vec = ld.position - pos;
        float distSq = dot(L_vec, L_vec);
        L = normalize(L_vec);
        attenuation = 1.0 / max(0.001, distSq);
    }

    float dotNL = max(0.0, dot(norm, L));
    return luminance(ld.color) * ld.intensity * dotNL * attenuation;
}

void updateReservoir(inout Reservoir r, uint light_index, float weight, float random) {
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
