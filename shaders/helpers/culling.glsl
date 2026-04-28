#ifndef HELPERS_CULLING_GLSL
#define HELPERS_CULLING_GLSL

// Project world-space AABB to screen-space rect in a specific clip space.
// Returns false if the AABB is entirely behind the camera.
bool projectAABB(vec3 aabbMin, vec3 aabbMax, mat4 vp, ivec2 hizSize, float expansion, out vec4 screenRect, out float nearestDepth) {
	vec2 minScreen = vec2(1e30);
	vec2 maxScreen = vec2(-1e30);
	nearestDepth = 1.0;
	int cornersInFront = 0;

	for (int i = 0; i < 8; i++) {
		vec3 corner = vec3(
			(i & 1) != 0 ? aabbMax.x : aabbMin.x,
			(i & 2) != 0 ? aabbMax.y : aabbMin.y,
			(i & 4) != 0 ? aabbMax.z : aabbMin.z
		);
		vec4 clip = vp * vec4(corner, 1.0);

		if (clip.w <= 0.0) {
			continue;
		}

		cornersInFront++;
		vec3 ndc = clip.xyz / clip.w;
		vec2 uv = ndc.xy * 0.5 + 0.5;

		minScreen = min(minScreen, uv);
		maxScreen = max(maxScreen, uv);

		float d = ndc.z * 0.5 + 0.5;
		nearestDepth = min(nearestDepth, d);
	}

	if (cornersInFront == 0) {
		screenRect = vec4(0.0);
		nearestDepth = 0.0;
		return false;
	}

	if (cornersInFront < 8) {
		screenRect = vec4(0.0, 0.0, 1.0, 1.0);
		nearestDepth = 0.0;
		return true;
	}

	if (maxScreen.x < 0.0 || minScreen.x > 1.0 || maxScreen.y < 0.0 || minScreen.y > 1.0) {
		screenRect = vec4(0.0);
		nearestDepth = 0.0;
		return false;
	}

	vec2 expand = expansion / vec2(hizSize);
	minScreen -= expand;
	maxScreen += expand;

	screenRect = clamp(vec4(minScreen, maxScreen), 0.0, 1.0);
	return true;
}

// Hi-Z occlusion check for a screened AABB.
bool isOccludedHiZ(vec4 screenRect, float nearestDepth, sampler2D hizTexture, ivec2 hizSize, int hizMipCount) {
	vec2 screenSize = screenRect.zw - screenRect.xy;
	if (screenSize.x * screenSize.y > 0.5) {
		return false;
	}

	vec2  pixelSize = screenSize * vec2(hizSize);
	float maxPixelDim = max(pixelSize.x, pixelSize.y);
	int   mipLevel = clamp(int(ceil(log2(max(maxPixelDim, 1.0) / 2.0))), 0, hizMipCount - 1);

	float hizDepth = 0.0;
	hizDepth = max(hizDepth, textureLod(hizTexture, screenRect.xy, float(mipLevel)).r);
	hizDepth = max(hizDepth, textureLod(hizTexture, screenRect.zy, float(mipLevel)).r);
	hizDepth = max(hizDepth, textureLod(hizTexture, screenRect.xw, float(mipLevel)).r);
	hizDepth = max(hizDepth, textureLod(hizTexture, screenRect.zw, float(mipLevel)).r);

	if (hizDepth < 0.0001) {
		return false;
	}

	return nearestDepth > hizDepth;
}

// Full world-space occlusion check.
bool isOccludedWorld(vec3 aabbMin, vec3 aabbMax, mat4 prevVP, sampler2D hizTexture, ivec2 hizSize, int hizMipCount, float expansion) {
	vec4 screenRect;
	float nearestDepth;
	if (!projectAABB(aabbMin, aabbMax, prevVP, hizSize, expansion, screenRect, nearestDepth)) {
		return false;
	}
	return isOccludedHiZ(screenRect, nearestDepth, hizTexture, hizSize, hizMipCount);
}

// Minimum pixel size check.
bool isTooSmall(vec3 aabbMin, vec3 aabbMax, mat4 model, mat4 viewProj, vec2 viewportSize, float minPixelSize) {
	if (minPixelSize <= 0.0) return false;

	vec3 corners[8];
	corners[0] = vec3(aabbMin.x, aabbMin.y, aabbMin.z);
	corners[1] = vec3(aabbMin.x, aabbMin.y, aabbMax.z);
	corners[2] = vec3(aabbMin.x, aabbMax.y, aabbMin.z);
	corners[3] = vec3(aabbMin.x, aabbMax.y, aabbMax.z);
	corners[4] = vec3(aabbMax.x, aabbMin.y, aabbMin.z);
	corners[5] = vec3(aabbMax.x, aabbMin.y, aabbMax.z);
	corners[6] = vec3(aabbMax.x, aabbMax.y, aabbMin.z);
	corners[7] = vec3(aabbMax.x, aabbMax.y, aabbMax.z);

	vec2 minP = vec2(2.0);
	vec2 maxP = vec2(-2.0);
	bool allBehind = true;

	for (int i = 0; i < 8; i++) {
		vec4 clipPos = viewProj * model * vec4(corners[i], 1.0);
		if (clipPos.w > 0.0) {
			vec2 ndc = clipPos.xy / clipPos.w;
			minP = min(minP, ndc);
			maxP = max(maxP, ndc);
			allBehind = false;
		}
	}

	if (allBehind) return true;

	vec2 screenSize = (maxP - minP) * 0.5 * viewportSize;
	return max(screenSize.x, screenSize.y) < minPixelSize;
}

#endif
