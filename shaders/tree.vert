#version 460 core

struct GrassInstance {
    vec4 pos_rot;   // xyz = world pos, w = rotation
    vec4 scale_seed_biome; // x = height, y = width, z = seed, w = biome index
};

layout(std430, binding = [[TREE_INSTANCES_BINDING]]) buffer TreeInstances {
    GrassInstance treeInstances[];
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
out float fIsLeaf;   // 0.0 for trunk, 1.0 for leaves
out float fCanopyAo;

// Simple hash for random values
float hash(uint x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return float(x) / 4294967295.0;
}

void main() {
    int idx = gl_InstanceID;
    vec3 basePos = treeInstances[idx].pos_rot.xyz;
    float baseRotation = treeInstances[idx].pos_rot.w;
    float height = treeInstances[idx].scale_seed_biome.x;
    float width = treeInstances[idx].scale_seed_biome.y;
    uint seed = uint(treeInstances[idx].scale_seed_biome.z);
    int biomeIdx = int(treeInstances[idx].scale_seed_biome.w);

    float dist = distance(viewPos, basePos);

    // Seed-based shape variations
    float leanAngle = (hash(seed + 101u) - 0.5) * 0.25;
    float leanDir = hash(seed + 202u) * 6.283185;
    vec3 leanVector = vec3(cos(leanDir), 0.0, sin(leanDir)) * leanAngle;

    float trunkHeightRatio = 0.65 + hash(seed + 303u) * 0.15; // Trunk spans 65%-80% of height
    float trunkHeight = height * trunkHeightRatio;

    int vertID = gl_VertexID;

    vec3 worldPos = basePos;
    vec3 normal = vec3(0.0, 1.0, 0.0);
    vec2 texCoords = vec2(0.0);
    float heightFactor = 0.0;
    float isLeaf = 0.0;
    float canopyAo = 1.0;

    // Wind parameters
    float windInf = biomeProps[biomeIdx].windInfluence;
    float rigidity = biomeProps[biomeIdx].rigidity;

    if (vertID < 96) {
        // --- TRUNK GEOMETRY (96 Vertices = 4 vertical segments x 4 sides x 6 verts per quad) ---
        isLeaf = 0.0;
        int segIdx = vertID / 24;          // 0 to 3
        int inSeg = vertID % 24;
        int sideIdx = inSeg / 6;           // 0 to 3
        int triVert = inSeg % 6;

        float u = 0.0;
        float v_local = 0.0;
        if (triVert == 0) { u = 0.0; v_local = 0.0; }
        else if (triVert == 1) { u = 1.0; v_local = 0.0; }
        else if (triVert == 2) { u = 0.0; v_local = 1.0; }
        else if (triVert == 3) { u = 1.0; v_local = 0.0; }
        else if (triVert == 4) { u = 1.0; v_local = 1.0; }
        else if (triVert == 5) { u = 0.0; v_local = 1.0; }

        float v = (float(segIdx) + v_local) / 4.0; // 0.0 to 1.0 along trunk
        heightFactor = v * trunkHeightRatio;

        float h = v * trunkHeight;

        // Radius tapering from base to trunk top
        float taper = mix(1.0, 0.25, v * v);
        float radius = width * 0.45 * taper;

        // Side angle
        float angle0 = baseRotation + (float(sideIdx) / 4.0) * 6.2831853;
        float angle1 = baseRotation + (float(sideIdx + 1) / 4.0) * 6.2831853;
        float currentAngle = mix(angle0, angle1, u);

        vec3 localOffset = vec3(cos(currentAngle) * radius, h, sin(currentAngle) * radius);
        localOffset += leanVector * h; // Lean along trunk

        // Apply trunk wind bending
        float bendAngle = 0.0;
        vec3 rotAxis = vec3(0.0, 1.0, 0.0);
        getWindDeflectionAngleAndAxis(basePos, dist, v, windInf * 0.5, rigidity * 2.0,
                                      globalProps.windMultiplier, globalProps.rigidityMultiplier,
                                      seed, bendAngle, rotAxis);

        vec3 bentOffset = rotateVector(localOffset, rotAxis, bendAngle);
        worldPos = basePos + bentOffset;

        // Trunk radial normal
        vec3 radialNorm = normalize(vec3(cos(currentAngle), 0.1, sin(currentAngle)));
        normal = rotateVector(radialNorm, rotAxis, bendAngle);
        texCoords = vec2(float(sideIdx) + u, v * 3.0);
        canopyAo = mix(0.5, 0.9, v);

    } else {
        // --- CANOPY LEAF CLUSTERS (192 Vertices = 8 clusters x 4 cards x 6 verts) ---
        isLeaf = 1.0;
        int leafVertID = vertID - 96;
        int clusterIdx = leafVertID / 24;  // 0 to 7 (8 leaf clusters)
        int inCluster = leafVertID % 24;
        int cardIdx = inCluster / 6;       // 0 to 3 cards per cluster
        int triVert = inCluster % 6;

        float u = 0.0;
        float v_local = 0.0;
        if (triVert == 0) { u = -1.0; v_local = 0.0; }
        else if (triVert == 1) { u = 1.0; v_local = 0.0; }
        else if (triVert == 2) { u = -1.0; v_local = 1.0; }
        else if (triVert == 3) { u = 1.0; v_local = 0.0; }
        else if (triVert == 4) { u = 1.0; v_local = 1.0; }
        else if (triVert == 5) { u = -1.0; v_local = 1.0; }

        // Canopy sphere center
        vec3 canopyCenter = basePos + vec3(leanVector.x * trunkHeight, trunkHeight + height * 0.15, leanVector.z * trunkHeight);

        // Cluster relative center position
        float clusterAngle = baseRotation + (float(clusterIdx) / 8.0) * 6.2831853 + hash(seed + uint(clusterIdx * 17)) * 0.5;
        float clusterElevation = (hash(seed + uint(clusterIdx * 31)) - 0.3) * 0.8;
        float clusterDist = width * (1.2 + hash(seed + uint(clusterIdx * 47)) * 0.8);

        vec3 clusterOffset = vec3(
            cos(clusterAngle) * clusterDist,
            clusterElevation * height * 0.3,
            sin(clusterAngle) * clusterDist
        );
        vec3 clusterCenter = canopyCenter + clusterOffset;

        // Card size and rotation
        float cardWidth = width * (1.4 + hash(seed + uint(clusterIdx * 59)) * 0.6);
        float cardHeight = width * (1.4 + hash(seed + uint(clusterIdx * 71)) * 0.6);

        float cardYaw = baseRotation + (float(cardIdx) / 4.0) * 3.14159265 + hash(seed + uint(clusterIdx * 13 + cardIdx)) * 0.5;
        vec3 cardRight = vec3(cos(cardYaw), 0.0, sin(cardYaw));
        vec3 cardUp = vec3(0.0, 1.0, 0.0);

        vec3 localCardPos = cardRight * (u * cardWidth * 0.5) + cardUp * ((v_local - 0.5) * cardHeight);
        vec3 unbentCardPos = clusterCenter + localCardPos;

        // Trunk & canopy wind bending
        float trunkBend = 0.0;
        vec3 rotAxis = vec3(0.0, 1.0, 0.0);
        getWindDeflectionAngleAndAxis(basePos, dist, 0.8, windInf, rigidity,
                                      globalProps.windMultiplier, globalProps.rigidityMultiplier,
                                      seed, trunkBend, rotAxis);

        // Leaf flutter high frequency sway
        float flutterFactor = sin((basePos.x + basePos.z) * 0.5 + time * 4.0 + float(clusterIdx)) * 0.15 * windInf;
        vec3 bentCardPos = rotateVector(unbentCardPos - basePos, rotAxis, trunkBend + flutterFactor) + basePos;

        worldPos = bentCardPos;

        // --- SPHERICAL NORMALS ---
        // Normal points outwards from canopy center for realistic volume lighting!
        vec3 sphereNormal = normalize(worldPos - canopyCenter);
        vec3 cardFaceNormal = normalize(vec3(-cardRight.z, 0.3, cardRight.x));

        // Blend spherical normal with card face normal
        normal = normalize(mix(cardFaceNormal, sphereNormal, 0.75));

        texCoords = vec2(u * 0.5 + 0.5, v_local);
        heightFactor = (worldPos.y - basePos.y) / max(height, 0.001);
        canopyAo = clamp(distance(worldPos, canopyCenter) / (width * 2.5), 0.3, 1.0);
    }

    fNormal = normal;
    fTexCoords = texCoords;
    fHeightFactor = heightFactor;
    fWorldPos = worldPos;
    fBiomeIdx = biomeIdx;
    fIsLeaf = isLeaf;
    fCanopyAo = canopyAo;

    gl_Position = projection * view * vec4(worldPos, 1.0);
}
