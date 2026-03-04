#ifndef TERRAIN_COMMON_GLSL
#define TERRAIN_COMMON_GLSL

// Dependencies: worldScale from Lighting/TerrainData UBO, viewPos from Lighting UBO

float perturbLevel(vec3 worldPos, vec3 forward, bool isShadowPass, float worldScaleVal, vec3 vPos, float tLevelMin, float tLevelMax) {
	if (isShadowPass) {
		return 0;
	}

	// 1. DISTANCE FACTOR (The Base)
	float dist = distance(vPos, worldPos);
	float distanceLevel = tLevelMax * exp(-dist * (0.005 / worldScaleVal));
	distanceLevel = clamp(distanceLevel, tLevelMin, tLevelMax);

	// 2. FOCUS FACTOR (The Cull)
	vec3  dirCamToVertex = normalize(worldPos - vPos);
	float align = dot(dirCamToVertex, forward);
	float focusThreshold = mix(0.65, 0.45, clamp(worldScaleVal / 10.0, 0.0, 1.0));
	float focusCull = mix(0.60, smoothstep(focusThreshold, 1.0, align), smoothstep(100, 250, dist));

	// 3. Y factor lock
	float distanceMin = 20 * worldScaleVal * smoothstep(10 * worldScaleVal, 0, abs(worldPos.y - vPos.y));

	// COMBINE
	float finalLevel = max(distanceMin, distanceLevel) * focusCull;

	return clamp(finalLevel / tLevelMax, 0.0, 1.0);
}

#endif // TERRAIN_COMMON_GLSL
