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
    if (abs(ndc.x) > 1.1 || abs(ndc.y) > 1.1) return false; // Out of frustum (rough)

    vec2 uv = ndc.xy * 0.5 + 0.5;

    // Calculate required mip level based on screen-space footprint
    float screenRadius = (radius * u_viewportHeight * uProjection[1][1]) / (2.0 * clipPos.w);
    float lod = log2(screenRadius * 2.0);
    lod = clamp(lod, 0.0, float(hizMipCount - 1));

    float depth = textureLod(hizTexture, uv, lod).r;

    // NDZ depth is usually [0, 1] where 0 is near, 1 is far.
    // However, our Hi-Z might be linear or reversed.
    // Assuming standard GL depth:
    return ndc.z > depth + 0.0001;
}

#endif // CULLING_GLSL
