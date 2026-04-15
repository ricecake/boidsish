#version 430 core

struct GrassInstance {
    vec4 pos_rot;   // xyz = world pos, w = rotation
    vec4 scale_seed_biome; // x = height, y = width, z = seed, w = biome index
};

layout(std430, binding = [[GRASS_INSTANCES_BINDING]]) buffer GrassInstances {
    GrassInstance instances[];
};

out int vInstanceID;

void main() {
    vInstanceID = gl_InstanceID;
    gl_Position = vec4(instances[gl_InstanceID].pos_rot.xyz, 1.0);
}
