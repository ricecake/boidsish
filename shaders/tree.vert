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
out float fIsLeaf;   // 0.0 for trunk & branch bark, 1.0 for foliage
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
    float leanAngle = (hash(seed + 101u) - 0.5) * 0.20;
    float leanDir = hash(seed + 202u) * 6.283185;
    vec3 leanVector = vec3(cos(leanDir), 0.0, sin(leanDir)) * leanAngle;

    float trunkHeightRatio = 0.55 + hash(seed + 303u) * 0.10; // Trunk height ratio (55%-65%)
    float trunkHeight = height * trunkHeightRatio;
    vec3 trunkForkPos = basePos + vec3(leanVector.x * trunkHeight, trunkHeight, leanVector.z * trunkHeight);

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

        // Radius tapering from base to trunk fork
        float taper = mix(1.0, 0.30, v);
        float radius = width * 0.40 * taper;

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

    } else if (vertID < 192) {
        // --- BRANCH GEOMETRY (96 Vertices = 4 main branches x 4 quads per branch = 24 verts per branch) ---
        isLeaf = 0.0;
        int branchVertID = vertID - 96;
        int branchIdx = branchVertID / 24;  // 0 to 3 (4 main branches)
        int inBranch = branchVertID % 24;
        int quadIdx = inBranch / 6;         // 0 to 3 segments along branch length
        int triVert = inBranch % 6;

        float u = 0.0;
        float v_local = 0.0;
        if (triVert == 0) { u = -1.0; v_local = 0.0; }
        else if (triVert == 1) { u = 1.0; v_local = 0.0; }
        else if (triVert == 2) { u = -1.0; v_local = 1.0; }
        else if (triVert == 3) { u = 1.0; v_local = 0.0; }
        else if (triVert == 4) { u = 1.0; v_local = 1.0; }
        else if (triVert == 5) { u = -1.0; v_local = 1.0; }

        float v = (float(quadIdx) + v_local) / 4.0; // 0.0 (trunk fork) to 1.0 (branch tip)

        // Branch direction extending outwards and upwards
        float branchYaw = baseRotation + (float(branchIdx) / 4.0) * 6.2831853 + hash(seed + uint(branchIdx * 13)) * 0.4;
        float branchPitch = 0.35 + hash(seed + uint(branchIdx * 19)) * 0.25; // Arch outwards and upwards
        float branchLength = width * (2.2 + hash(seed + uint(branchIdx * 29)) * 0.8);

        vec3 branchDir = vec3(cos(branchYaw) * cos(branchPitch), sin(branchPitch), sin(branchYaw) * cos(branchPitch));
        vec3 branchRight = normalize(vec3(-sin(branchYaw), 0.0, cos(branchYaw)));

        float branchThickness = width * 0.18 * (1.0 - v * 0.7);

        vec3 unbentBranchPos = trunkForkPos + branchDir * (v * branchLength) + branchRight * (u * branchThickness);

        // Apply wind deflection along branch
        float bendAngle = 0.0;
        vec3 rotAxis = vec3(0.0, 1.0, 0.0);
        getWindDeflectionAngleAndAxis(basePos, dist, 0.6 + v * 0.4, windInf * 0.7, rigidity * 1.5,
                                      globalProps.windMultiplier, globalProps.rigidityMultiplier,
                                      seed, bendAngle, rotAxis);

        worldPos = rotateVector(unbentBranchPos - basePos, rotAxis, bendAngle) + basePos;

        vec3 branchNormal = normalize(cross(branchDir, branchRight));
        normal = rotateVector(branchNormal, rotAxis, bendAngle);
        texCoords = vec2(u * 0.5 + 0.5, v * 2.0);
        canopyAo = mix(0.7, 0.95, v);

    } else {
        // --- FOLIAGE / LEAF CLUSTERS (192 Vertices = 8 clusters x 4 cards x 6 verts) ---
        isLeaf = 1.0;
        int leafVertID = vertID - 192;
        int clusterIdx = leafVertID / 24;  // 0 to 7 (8 leaf clusters)
        int inCluster = leafVertID % 24;
        int cardIdx = inCluster / 6;       // 0 to 3 leaf cards per cluster
        int triVert = inCluster % 6;

        float u = 0.0;
        float v_local = 0.0;
        if (triVert == 0) { u = -1.0; v_local = 0.0; }
        else if (triVert == 1) { u = 1.0; v_local = 0.0; }
        else if (triVert == 2) { u = -1.0; v_local = 1.0; }
        else if (triVert == 3) { u = 1.0; v_local = 0.0; }
        else if (triVert == 4) { u = 1.0; v_local = 1.0; }
        else if (triVert == 5) { u = -1.0; v_local = 1.0; }

        // Associated branch for this cluster (2 clusters per branch)
        int assocBranch = clusterIdx / 2;
        float branchYaw = baseRotation + (float(assocBranch) / 4.0) * 6.2831853 + hash(seed + uint(assocBranch * 13)) * 0.4;
        float branchPitch = 0.35 + hash(seed + uint(assocBranch * 19)) * 0.25;
        float branchLength = width * (2.2 + hash(seed + uint(assocBranch * 29)) * 0.8);
        vec3 branchDir = vec3(cos(branchYaw) * cos(branchPitch), sin(branchPitch), sin(branchYaw) * cos(branchPitch));

        // Cluster center positioned along and at the tips of branches!
        float distAlongBranch = (clusterIdx % 2 == 0) ? 0.65 : 1.0; // Mid-branch and tip-branch clusters
        vec3 branchClusterBase = trunkForkPos + branchDir * (distAlongBranch * branchLength);

        float clusterSpread = width * (0.8 + hash(seed + uint(clusterIdx * 37)) * 0.6);
        float clusterAngle = (float(clusterIdx) / 8.0) * 6.2831853 + hash(seed + uint(clusterIdx * 41)) * 0.5;
        vec3 clusterOffset = vec3(cos(clusterAngle) * clusterSpread, hash(seed + uint(clusterIdx * 53)) * height * 0.15, sin(clusterAngle) * clusterSpread);

        vec3 clusterCenter = branchClusterBase + clusterOffset;
        vec3 canopyCenter = trunkForkPos + vec3(0.0, height * 0.2, 0.0);

        // Leafy frond / card styling: anisotropic leaf sprigs extending outward
        float cardWidth = width * (1.2 + hash(seed + uint(clusterIdx * 59)) * 0.5);
        float cardHeight = width * (1.5 + hash(seed + uint(clusterIdx * 71)) * 0.5);

        float cardYaw = branchYaw + (float(cardIdx) / 4.0) * 3.14159265 + hash(seed + uint(clusterIdx * 13 + cardIdx)) * 0.6;
        vec3 cardRight = vec3(cos(cardYaw), 0.0, sin(cardYaw));
        vec3 cardUp = vec3(0.0, 1.0, 0.0);

        vec3 localCardPos = cardRight * (u * cardWidth * 0.5) + cardUp * ((v_local - 0.2) * cardHeight);
        vec3 unbentCardPos = clusterCenter + localCardPos;

        // Trunk & branch wind bending
        float trunkBend = 0.0;
        vec3 rotAxis = vec3(0.0, 1.0, 0.0);
        getWindDeflectionAngleAndAxis(basePos, dist, 0.8, windInf, rigidity,
                                      globalProps.windMultiplier, globalProps.rigidityMultiplier,
                                      seed, trunkBend, rotAxis);

        // High frequency leaf flutter
        float flutterFactor = sin((basePos.x + basePos.z) * 0.5 + time * 5.0 + float(clusterIdx)) * 0.18 * windInf;
        vec3 bentCardPos = rotateVector(unbentCardPos - basePos, rotAxis, trunkBend + flutterFactor) + basePos;

        worldPos = bentCardPos;

        // --- SPHERICAL NORMALS ---
        // Normal points outward from the central canopy center for volumetric shading
        vec3 sphereNormal = normalize(worldPos - canopyCenter);
        vec3 cardFaceNormal = normalize(vec3(-cardRight.z, 0.4, cardRight.x));

        normal = normalize(mix(cardFaceNormal, sphereNormal, 0.70));

        texCoords = vec2(u * 0.5 + 0.5, v_local);
        heightFactor = (worldPos.y - basePos.y) / max(height, 0.001);
        canopyAo = clamp(distance(worldPos, canopyCenter) / (width * 3.0), 0.35, 1.0);
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
