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
 * should be rendered based on its screen-space pixel size and relative luminosity.
 *
 * @param worldSize The size of the object in world units (radius or bounding sphere).
 * @param luminosity The brightness of the object [0, 1+].
 * @return The maximum render distance in world units.
 */
float getMaxRenderDistance(float worldSize, float emissiveLuminosity) {
    // 1. Perceived Luminosity and Contrast
    // Total luminosity of the object (emissive + estimated reflected ambient).
    // We assume a base reflectiveness of 0.2 for generic objects like trees.
    float environmentLuminance = dot(ambient_light, vec3(0.2126, 0.7152, 0.0722));
    float perceivedLuminosity = emissiveLuminosity + 0.2 * environmentLuminance;

    // Absolute darkness threshold: if it's too dark to see at all, cull it.
    // Increased threshold to 0.002 (~0.5/255) for more aggressive culling of dark objects.
    if (perceivedLuminosity < 0.002) return 0.0;

    // Contrast ratio against background.
    // In many settings, background is roughly environment ambient.
    float contrast = abs(perceivedLuminosity - environmentLuminance) / max(environmentLuminance, 0.001);

    // Minimum contrast threshold for visibility.
    // Increased to 0.05 (5%) to cull low-contrast objects earlier.
    if (contrast < 0.05) return 0.0;

    // 2. Minimum Pixel Size Culling
    // Distance at which the object reaches the minimum required pixel size.
    float minSize = max(0.1, u_minPixelSize);
    float baseDist = (worldSize * u_viewportHeight * uProjection[1][1]) / (2.0 * minSize);

    // 3. Visibility Scaling
    // Contrast-based scaling: high contrast (bright fireflies at night) extends range.
    // Low contrast/low luminance (dark trees in dark night) reduces range.
    float visibilityFactor = sqrt(contrast) * perceivedLuminosity / max(environmentLuminance, 0.1);

    // nightVisionBoost scales how much this relative factor affects the result.
    // Allow visibilityFactor to go all the way to 0.0 to completely skip objects.
    float factor = mix(1.0, clamp(visibilityFactor, 0.0, 5.0), nightFactor * u_nightVisionBoost);

    return baseDist * factor;
}

/**
 * Performs a Hi-Z occlusion test for a bounding sphere.
 */
bool isOccluded(vec3 center, float radius, sampler2D hizTexture, ivec2 hizSize, int hizMipCount) {
    // Use prevViewProjection because hizTexture is from the previous frame
    vec4 clipPos = prevViewProjection * vec4(center, 1.0);
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
