#ifndef HELPERS_DEPTH_GLSL
#define HELPERS_DEPTH_GLSL

/**
 * Linearize depth from the depth buffer.
 * Converts from non-linear depth buffer value to linear view-space distance.
 *
 * @param depth The depth value from the depth buffer (0.0 to 1.0).
 * @param near The linear distance to the near plane.
 * @param far The linear distance to the far plane.
 * @return The linear view-space distance.
 */
float linearizeDepth(float depth, float near, float far) {
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

#endif // HELPERS_DEPTH_GLSL
