#ifndef VOLUMETRIC_COMMON_GLSL
#define VOLUMETRIC_COMMON_GLSL

const int NUM_CASCADES = 4;
const float CASCADE_DISTANCES[4] = { 20.0, 60.0, 200.0, 1000.0 };

// Using 1000000.0 as scale for fixed point atomics
const float FIXED_POINT_SCALE = 1000000.0;

struct VolumetricData {
    uint radiance_r;
    uint radiance_g;
    uint radiance_b;
    uint extinction;
};

ivec3 worldToFroxel(vec3 worldPos, mat4 view, mat4 proj, ivec3 grid_res) {
    vec4 viewPos = view * vec4(worldPos, 1.0);
    float linearZ = -viewPos.z;

    int cascade = -1;
    float z_near = 0.1;
    float z_far = 0.0;

    for (int i = 0; i < NUM_CASCADES; ++i) {
        if (linearZ <= CASCADE_DISTANCES[i]) {
            cascade = i;
            z_far = CASCADE_DISTANCES[i];
            if (i > 0) z_near = CASCADE_DISTANCES[i-1];
            break;
        }
    }

    if (cascade == -1 || linearZ < z_near) return ivec3(-1);

    vec4 clipPos = proj * viewPos;
    vec3 ndc = clipPos.xyz / clipPos.w;
    if (any(lessThan(ndc, vec3(-1.1))) || any(greaterThan(ndc, vec3(1.1)))) return ivec3(-1);

    vec2 uv = ndc.xy * 0.5 + 0.5;
    float slice = clamp(log(linearZ / z_near) / log(z_far / z_near), 0.0, 1.0);

    return ivec3(
        int(uv.x * float(grid_res.x)),
        int(uv.y * float(grid_res.y)),
        cascade * grid_res.z + int(slice * float(grid_res.z))
    );
}

vec3 worldFromFroxel(ivec3 vox, vec3 jitter, mat4 invView, mat4 invProj, ivec3 grid_res) {
    int cascade = vox.z / grid_res.z;
    int z_in_cascade = vox.z % grid_res.z;

    vec3 ndc;
    ndc.xy = (vec2(vox.xy) + jitter.xy) / vec2(grid_res.xy) * 2.0 - 1.0;

    float z_near = (cascade == 0) ? 0.1 : CASCADE_DISTANCES[cascade - 1];
    float z_far = CASCADE_DISTANCES[cascade];

    // Logarithmic distribution for planar Depth (Z)
    float linear_z = z_near * pow(z_far / z_near, (float(z_in_cascade) + jitter.z) / float(grid_res.z));

    // Reconstruct View Space Position using planar depth
    vec4 clipPos = vec4(ndc.xy, 0.0, 1.0);
    vec4 viewDir = invProj * clipPos;
    viewDir /= (abs(viewDir.w) > 0.0001) ? viewDir.w : 1.0;

    // Planar depth reconstruction: P.z = -linear_z (assuming OpenGL RH)
    float obliquity = length(viewDir.xyz) / max(0.0001, -viewDir.z);
    vec3 froxelViewPos = viewDir.xyz * (linear_z / max(0.0001, -viewDir.z));

    return (invView * vec4(froxelViewPos, 1.0)).xyz;
}

float getObliquity(ivec2 voxXY, vec3 jitter, mat4 invProj, ivec3 grid_res) {
    vec3 ndc;
    ndc.xy = (vec2(voxXY) + jitter.xy) / vec2(grid_res.xy) * 2.0 - 1.0;
    vec4 clipPos = vec4(ndc.xy, 0.0, 1.0);
    vec4 viewDir = invProj * clipPos;
    viewDir /= (abs(viewDir.w) > 0.0001) ? viewDir.w : 1.0;
    return length(viewDir.xyz) / max(0.0001, -viewDir.z);
}

#endif
