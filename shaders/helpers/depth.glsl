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

/**
 * Reconstruct view-space position from screen UV and depth.
 *
 * @param uv Normalized screen coordinates (0.0 to 1.0).
 * @param depth Raw depth buffer value (0.0 to 1.0).
 * @param invProj Inverse projection matrix.
 * @return View-space position.
 */
vec3 reconstructViewPos(vec2 uv, float depth, mat4 invProj) {
	vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPos = invProj * clipPos;
	return viewPos.xyz / viewPos.w;
}

/**
 * Reconstruct world-space position from screen UV and depth.
 *
 * @param uv Normalized screen coordinates (0.0 to 1.0).
 * @param depth Raw depth buffer value (0.0 to 1.0).
 * @param invProj Inverse projection matrix.
 * @param invView Inverse view matrix.
 * @return World-space position.
 */
vec3 reconstructWorldPos(vec2 uv, float depth, mat4 invProj, mat4 invView) {
	vec3 viewPos = reconstructViewPos(uv, depth, invProj);
	return (invView * vec4(viewPos, 1.0)).xyz;
}

/**
 * Calculate a world-space view ray direction for a given screen UV.
 *
 * @param uv Normalized screen coordinates (0.0 to 1.0).
 * @param invProj Inverse projection matrix.
 * @param invView Inverse view matrix.
 * @return Normalized world-space ray direction.
 */
vec3 getViewRay(vec2 uv, mat4 invProj, mat4 invView) {
	// Near plane point in view space
	vec4 nearClip = vec4(uv * 2.0 - 1.0, -1.0, 1.0);
	vec4 nearView = invProj * nearClip;
	nearView /= nearView.w;

	// Point slightly further along Z
	vec4 farClip = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
	vec4 farView = invProj * farClip;
	farView /= farView.w;

	vec3 viewDir = normalize(farView.xyz - nearView.xyz);
	return normalize(mat3(invView) * viewDir);
}

/**
 * Project a world-space position to screen UV and depth.
 *
 * @param worldPos World-space position to project.
 * @param viewProj Combined view-projection matrix.
 * @return vec3 containing (u, v, raw_depth).
 */
vec3 projectToScreen(vec3 worldPos, mat4 viewProj) {
	vec4 clipPos = viewProj * vec4(worldPos, 1.0);
	vec3 ndc = clipPos.xyz / clipPos.w;
	return vec3(ndc.xy * 0.5 + 0.5, ndc.z * 0.5 + 0.5);
}

#endif // HELPERS_DEPTH_GLSL
