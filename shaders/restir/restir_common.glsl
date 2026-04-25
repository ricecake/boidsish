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

uniform int u_num_lights;
uniform int u_num_fire_particles;

LightData getLightData(uint index) {
    LightData ld;
    if (index < uint(u_num_lights)) {
        ld.position = gpu_lights[index].position;
        ld.color = gpu_lights[index].color;
        ld.intensity = gpu_lights[index].intensity;
        ld.type = gpu_lights[index].type;
    } else {
        uint p_idx = index - uint(u_num_lights);
        ld.position = particles[p_idx].pos.xyz;
        // Fire particles are typically orange/yellow/white
        ld.color = vec3(1.0, 0.6, 0.2);
        ld.intensity = 1.0; // Scaled in BRDF
        ld.type = 0; // POINT
    }
    return ld;
}

float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

float targetFunction(vec3 pos, vec3 norm, LightData ld) {
    vec3 L = ld.position - pos;
    float distSq = dot(L, L);
    L = normalize(L);
    float dotNL = max(0.0, dot(norm, L));
    return luminance(ld.color) * ld.intensity * dotNL / max(0.001, distSq);
}

void updateReservoir(inout Reservoir r, uint light_index, float weight, float random) {
    r.w_sum += weight;
    r.m += 1.0;
    if (random * r.w_sum < weight) {
        r.light_index = light_index;
    }
}

#endif
