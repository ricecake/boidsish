#version 460 core

struct GrassInstance {
    vec4 pos_rot;   // xyz = world pos, w = rotation
    vec4 scale_seed_biome; // x = height, y = width, z = seed, w = biome index
};

layout(std430, binding = [[GRASS_INSTANCES_BINDING]]) buffer GrassInstances {
    GrassInstance instances[];
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
out float fIsFlower;

uniform vec3 uCameraPos;

// Simple hash for random values
float hash(uint x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return float(x) / 4294967295.0;
}

void main() {
    int idx = gl_InstanceID;
    vec3 basePos = instances[idx].pos_rot.xyz;
    float baseRotation = instances[idx].pos_rot.w;
    float height = instances[idx].scale_seed_biome.x;
    float width = instances[idx].scale_seed_biome.y;
    uint seed = uint(instances[idx].scale_seed_biome.z);
    int biomeIdx = int(instances[idx].scale_seed_biome.w);
    float dist = distance(uCameraPos, basePos);

    float flowerChance = hash(seed + 1111);
    bool isFlower = (flowerChance < biomeProps[biomeIdx].flowerRatio);
    fIsFlower = isFlower ? 1.0 : 0.0;

    if (isFlower) {
        height *= 1.35;
    }

    // Compute vertex index parts for vertex pulling (5 segments, 30 vertices total)
    int segmentIdx = gl_VertexID / 6;        // 0 to 4 (5 segments)
    int triVertIdx = gl_VertexID % 6;        // 0 to 5

    // TriVert mapping to segment local (u, v_local)
    float u = 0.0;
    float v_local = 0.0;
    if (triVertIdx == 0) { u = -1.0; v_local = 0.0; }
    else if (triVertIdx == 1) { u = 1.0; v_local = 0.0; }
    else if (triVertIdx == 2) { u = -1.0; v_local = 1.0; }
    else if (triVertIdx == 3) { u = 1.0; v_local = 0.0; }
    else if (triVertIdx == 4) { u = 1.0; v_local = 1.0; }
    else if (triVertIdx == 5) { u = -1.0; v_local = 1.0; }

    float v = (float(segmentIdx) + v_local) / 5.0; // overall v along blade (0 to 1)

    width += 3.0 * width * smoothstep(35.0, 75.0, dist);

    // Wind using our new shared helper!
    float totalBendAngle = 0.0;
    vec3 rotationAxis = vec3(0.0, 1.0, 0.0);
    getWindDeflectionAngleAndAxis(basePos, dist, v, biomeProps[biomeIdx].windInfluence, biomeProps[biomeIdx].rigidity, globalProps.windMultiplier, globalProps.rigidityMultiplier, seed, totalBendAngle, rotationAxis);

    // Initial local position before bending (vertical segment)
    float segmentHeight = v * height;
    vec3 initialRelativePos = vec3(0.0, segmentHeight, 0.0);

    // Bending using our new helper!
    vec3 bentRelativePos = rotateVector(initialRelativePos, rotationAxis, totalBendAngle);

    // --- Blade Width and Rotation ---
    // Distance-based weight
    float billboardWeight = max(0.75, smoothstep(5.50, 50.0, dist));

    // Original facing direction
    vec2 origDir = vec2(cos(baseRotation), sin(baseRotation));

    // Camera facing direction (XZ plane only)
    vec2 camDir = normalize(uCameraPos.xz - basePos.xz);

    // Normalized linear interpolation (nlerp) of the directions
    vec2 finalDir = normalize(mix(origDir, camDir, billboardWeight));

    vec2 forward = finalDir;
    vec2 right = vec2(forward.y, -forward.x); // 2D cross product ensures perpendicularity

    // --- Flower Logic ---
    // Taper curve: 1.0 at the base (v=0), 0.0 at the tip (v=1).
    float shapeExponent = mix(2.0, 4.0, smoothstep(32.0, 128.0, dist));
    float shapeProfile = 1.0 - pow(v, shapeExponent);

    // Artificially widen the entire blade at distance to preserve density
    float distanceWidthBoost = mix(1.0, 2.0, smoothstep(32.0, 128.0, dist));

    float currentWidth;
    if (isFlower) {
        // Narrow stem at the base, multiple head shapes at the top
        float stemHeight = 0.75;
        if (v < stemHeight) {
            currentWidth = u * width * 0.2; // Very narrow stem
        } else {
            // Re-normalize v for the head [0, 1]
            float vHead = (v - stemHeight) / (1.0 - stemHeight);
            uint shapeSeed = seed % 4u;
            float headProfile;

            if (shapeSeed == 0u) { // Diamond
                headProfile = 1.0 - abs(vHead * 2.0 - 1.0);
            } else if (shapeSeed == 1u) { // Bell
                headProfile = pow(sin(vHead * 3.14159), 0.5);
            } else if (shapeSeed == 2u) { // Star-like
                headProfile = abs(sin(vHead * 3.14159 * 2.0));
                headProfile = mix(headProfile, 1.0 - vHead, 0.3);
            } else { // Heart/Wide Diamond
                headProfile = (1.0 - vHead) * pow(vHead, 0.3) * 2.5;
            }

            currentWidth = u * width * 2.5 * headProfile;
        }
    } else {
        currentWidth = u * width * 0.5 * shapeProfile;
    }
    currentWidth *= distanceWidthBoost;

    vec2 localXZ = right * currentWidth;

    // Final World Position
    vec3 pos = basePos + bentRelativePos;
    pos.xz += localXZ;

    vec3 faceNormal = vec3(forward.x, 0.0, forward.y);
    vec3 sideVec = vec3(right.x, 0.0, right.y);
    fNormal = normalize(faceNormal + sideVec * u * 0.8 + vec3(0.0, v * 0.3, 0.0));

    fTexCoords = vec2(u * 0.5 + 0.5, v);
    fHeightFactor = v;
    fWorldPos = pos;
    fBiomeIdx = biomeIdx;

    gl_Position = projection * view * vec4(pos, 1.0);
}
