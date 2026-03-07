#ifndef CULLING_GLSL
#define CULLING_GLSL

#include "temporal_data.glsl"
#include "helpers/lighting.glsl"

layout(std140, binding = 9) uniform CullingDataBlock {
    float u_minPixelSize;
    float u_nightVisionBoost;
    float u_viewportHeight;
    float u_paddingCulling;
};

/**
 * Calculates the maximum distance at which an object of a given size and luminosity
 * should be rendered to maintain a minimum screen-space pixel size.
 *
 * @param worldSize The size of the object in world units (radius or bounding sphere).
 * @param luminosity The brightness of the object [0, 1+].
 * @return The maximum render distance in world units.
 */
float getMaxRenderDistance(float worldSize, float luminosity) {
    // Project world size to pixel size:
    // pixelSize = (worldSize * viewportHeight * projection[1][1]) / (2.0 * distance * tan(fov/2))
    // We use projection[1][1] directly as it encodes focal length.

    // Boost render distance for bright objects at night
    float effectiveLuminosity = max(0.1, luminosity);
    float lightBoost = mix(1.0, 1.0 + effectiveLuminosity * u_nightVisionBoost, nightFactor);

    float minSize = max(0.1, u_minPixelSize);

    // Distance = (worldSize * viewportHeight * proj[1][1]) / (2.0 * minSize)
    // We multiply by lightBoost to extend the range for bright objects.
    float dist = (worldSize * u_viewportHeight * uProjection[1][1] * lightBoost) / (2.0 * minSize);

    return dist;
}

/**
 * Performs a Hi-Z occlusion test for a bounding sphere.
 */
bool isOccluded(vec3 center, float radius, sampler2D hizTexture, ivec2 hizSize, int hizMipCount) {
    vec4 clipPos = viewProjection * vec4(center, 1.0);
    if (clipPos.w <= 0.0) return false; // Behind camera

    vec3 ndc = clipPos.xyz / clipPos.w;
    // Map NDC Z from [-1, 1] to [0, 1] for depth comparison
    float nearestDepth = ndc.z * 0.5 + 0.5;

    if (abs(ndc.x) > 1.1 || abs(ndc.y) > 1.1) return false; // Out of frustum (rough)

    vec2 uv = ndc.xy * 0.5 + 0.5;

    // Calculate required mip level based on screen-space footprint
    // Screen-space radius in pixels
    float screenRadius = (radius * u_viewportHeight * uProjection[1][1]) / (2.0 * clipPos.w);
    float maxPixelDim = screenRadius * 2.0;

    // Calculate LOD based on size
    int lod = clamp(int(ceil(log2(max(maxPixelDim, 1.0) / 2.0))), 0, hizMipCount - 1);

    // Perform conservative Hi-Z test by sampling 4 texels at the appropriate LOD
    // This is more robust than a single point sample
    float offset = screenRadius / float(hizSize.x); // Approximate UV offset

    float hizDepth = 0.0;
    hizDepth = max(hizDepth, textureLod(hizTexture, uv + vec2(-offset, -offset), float(lod)).r);
    hizDepth = max(hizDepth, textureLod(hizTexture, uv + vec2(offset, -offset), float(lod)).r);
    hizDepth = max(hizDepth, textureLod(hizTexture, uv + vec2(-offset, offset), float(lod)).r);
    hizDepth = max(hizDepth, textureLod(hizTexture, uv + vec2(offset, offset), float(lod)).r);

    // If Hi-Z reads as zero/near-zero, the data is invalid (uninitialized or
    // stale due to a memory barrier issue). Conservatively keep the object visible.
    if (hizDepth < 0.0001)
        return false;

    return nearestDepth > hizDepth + 0.0001;
}

#endif // CULLING_GLSL
