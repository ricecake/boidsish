#version 460 core

struct GrassInstance {
    vec4 pos_rot;   // xyz = world pos, w = rotation
    vec4 scale_seed_biome; // x = height, y = width, z = seed, w = biome index
};

layout(std430, binding = [[FOLIAGE_INSTANCES_BINDING]]) buffer FoliageInstances {
    GrassInstance foliageInstances[];
};

struct GrassProperties {
    vec4  colorTop;
    vec4  colorBottom;
    float height;
    float width;
    float rigidity;
    float heightVariance;
    float widthVariance;
    float density;
    float colorVariability;
    float windInfluence;
    uint  enabled;
    float flowerRatio;
    float _pad1;
    float _pad2;
};

struct GlobalGrassProperties {
    float lengthMultiplier;
    float widthMultiplier;
    float densityMultiplier;
    float rigidityMultiplier;
    float windMultiplier;
    uint  enabled;
    float lodScaleFactor;
    float lodBaseRange;
    float baseScale;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std140, binding = [[GRASS_PROPS_BINDING]]) uniform GrassProps {
    GrassProperties biomeProps[8];
    GlobalGrassProperties globalProps;
};

#include "lighting.glsl"
#include "helpers/wind.glsl"

out vec3 fNormal;
out vec2 fTexCoords;
out float fHeightFactor;
out vec3 fWorldPos;
flat out int fBiomeIdx;

// Simple hash for random values
float hash(uint x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return float(x) / 4294967295.0;
}

void main() {
    int idx = gl_InstanceID;
    vec3 basePos = foliageInstances[idx].pos_rot.xyz;
    float baseRotation = foliageInstances[idx].pos_rot.w;
    float height = foliageInstances[idx].scale_seed_biome.x;
    float width = foliageInstances[idx].scale_seed_biome.y;
    uint seed = uint(foliageInstances[idx].scale_seed_biome.z);
    int biomeIdx = int(foliageInstances[idx].scale_seed_biome.w);

    // Compute vertex index parts for vertex pulling
    int frondIdx = gl_VertexID / 24;            // 0 to 5 (6 fronds)
    int vertIdxInFrond = gl_VertexID % 24;      // 0 to 23
    int segmentIdx = vertIdxInFrond / 6;        // 0 to 3 (4 segments)
    int triVertIdx = vertIdxInFrond % 6;        // 0 to 5

    // TriVert mapping to segment local (u, v_local)
    float u = 0.0;
    float v_local = 0.0;
    if (triVertIdx == 0) { u = -1.0; v_local = 0.0; }
    else if (triVertIdx == 1) { u = 1.0; v_local = 0.0; }
    else if (triVertIdx == 2) { u = -1.0; v_local = 1.0; }
    else if (triVertIdx == 3) { u = 1.0; v_local = 0.0; }
    else if (triVertIdx == 4) { u = 1.0; v_local = 1.0; }
    else if (triVertIdx == 5) { u = -1.0; v_local = 1.0; }

    float v = (float(segmentIdx) + v_local) / 4.0; // overall v along frond (0 to 1)

    // Base frond yaw spacing
    float frondYaw = (float(frondIdx) / 6.0) * 6.2831853;
    float seedOffset = hash(seed + uint(frondIdx * 123));
    float yaw = baseRotation + frondYaw + (seedOffset * 2.0 - 1.0) * 0.15;

    vec3 forward = vec3(sin(yaw), 0.0, cos(yaw));
    vec3 right = vec3(cos(yaw), 0.0, -sin(yaw));

    // Wind influence & deflection
    float dist = distance(viewPos, basePos);
    float totalBendAngle = 0.0;
    vec3 rotationAxis = vec3(0.0, 1.0, 0.0);
    getWindDeflectionAngleAndAxis(basePos, dist, v, biomeProps[biomeIdx].windInfluence, biomeProps[biomeIdx].rigidity, globalProps.windMultiplier, globalProps.rigidityMultiplier, seed, totalBendAngle, rotationAxis);

    // Arch curve of the fern frond (pointing outwards and arching down under gravity)
    float heightScale = height * 0.5; // low frond
    float frondLength = height * 1.2;

    float x_forward = v * frondLength;
    // Arching equation
    float y_up = heightScale * (sin(v * 2.0) - 0.3 * v * v);

    // Tapering width profile (starts thin, gets wider at 0.3, then tapers to 0 at tip)
    float widthProfile = smoothstep(0.0, 0.3, v) * (1.0 - v);
    float frondWidth = width * 1.8 * widthProfile;

    // Unbent local relative position
    vec3 initialRelativePos = forward * x_forward + vec3(0.0, y_up, 0.0) + right * (u * frondWidth);

    // Apply wind deflection using the shared Rodrigues' rotation helper
    vec3 bentRelativePos = rotateVector(initialRelativePos, rotationAxis, totalBendAngle);

    // Final vertex position
    vec3 pos = basePos + bentRelativePos;

    // Compute approximate unbent normal
    vec3 tangent = normalize(forward * frondLength + vec3(0.0, heightScale * (2.0 * cos(v * 2.0) - 0.6 * v), 0.0));
    vec3 norm = normalize(cross(tangent, right));
    if (norm.y < 0.0) norm = -norm; // Ensure normal points generally upwards

    // Rotate normal with the same deflection
    norm = rotateVector(norm, rotationAxis, totalBendAngle);

    fNormal = norm;
    fTexCoords = vec2(u * 0.5 + 0.5, v);
    fHeightFactor = v;
    fWorldPos = pos;
    fBiomeIdx = biomeIdx;

    gl_Position = projection * view * vec4(pos, 1.0);
}
