#version 430 core

layout(location = 0) out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform usampler2DArray sssTileMask;

#include "../temporal_data.glsl"
#include "../lighting.glsl"

// Assume this is defined in your includes or elsewhere
// float fastBlueNoise(vec2 uv);

vec3 worldPosFromDepth(float d) {
	vec4 clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
	vec4 worldSpacePosition = invView * invProjection * clipSpacePosition;
	return worldSpacePosition.xyz / worldSpacePosition.w;
}

// Helper to extract linear view-space Z from the depth buffer
// This completely neutralizes the non-linear perspective warping for thickness checks
float getViewZ(float depth) {
    vec4 clip = vec4(0.0, 0.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjection * clip;
    return view.z / view.w;
}

uniform bool uDebugSSS = false;

void main() {
	if (uDebugSSS) {
		FragColor = vec4(1.0, 0.0, 1.0, 1.0);
		return;
	}

	float d = texture(depthTexture, TexCoords).r;
	if (d >= 1.0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

	vec3 worldPos = worldPosFromDepth(d);
	vec3 combinedShadow = vec3(0.0);
	vec2 texRes = vec2(textureSize(depthTexture, 0));

	for (int i = 0; i < num_lights; ++i) {
		int shadow_index = lightShadowIndices[i];
		if (shadow_index < 0) continue;

		// Find appropriate cascade for directional lights
		int cascade = 0;
		if (lights[i].type == 1) { // DIRECTIONAL
			float depth = dot(worldPos - viewPos, viewDir);
			cascade = -1;
			for (int c = 0; c < MAX_CASCADES; ++c) {
				if (depth < cascadeSplits[c]) {
					cascade = c;
					break;
				}
			}
			if (cascade == -1) cascade = MAX_CASCADES - 1;
			shadow_index += cascade;
		}

		if (shadow_index >= MAX_SHADOW_MAPS) continue;

		// Tile-based optimization check
		vec4 lightSpacePos = lightSpaceMatrices[shadow_index] * vec4(worldPos, 1.0);
		vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
		projCoords = projCoords * 0.5 + 0.5;

		if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) continue;

		uint maskValue = texture(sssTileMask, vec3(projCoords.xy, float(shadow_index))).r;
		if (maskValue == 0) continue;

		// Tracing needed for this light
		vec3 lightDir;
		if (lights[i].type == 1) {
			lightDir = normalize(-lights[i].direction);
		} else {
			lightDir = normalize(lights[i].position - worldPos);
		}

		float maxRayDistance = 1.5 * worldScale;
		vec3 rayEndWorld = worldPos + (lightDir * maxRayDistance);

		vec4 startClip = viewProjection * vec4(worldPos, 1.0);
		vec4 endClip = viewProjection * vec4(rayEndWorld, 1.0);

		startClip.w = max(startClip.w, 0.0001);
		endClip.w = max(endClip.w, 0.0001);

		vec3 startNDC = startClip.xyz / startClip.w;
		vec3 endNDC = endClip.xyz / endClip.w;

		vec2 startUV = startNDC.xy * 0.5 + 0.5;
		vec2 endUV = endNDC.xy * 0.5 + 0.5;

		vec2 deltaUV = endUV - startUV;
		vec2 pixelDistance = abs(deltaUV * texRes);

		float steps = max(pixelDistance.x, pixelDistance.y);

		// Scale quality by cascade index: higher cascade = fewer steps
		float qualityMultiplier = 1.0 / (1.0 + float(cascade));
		steps = min(steps, 128.0 * qualityMultiplier);

		if (steps < 1.0) continue;

		vec2 stepUV = deltaUV / steps;
		float startInvW = 1.0 / startClip.w;
		float endInvW = 1.0 / endClip.w;
		float startZInvW = startClip.z / startClip.w;
		float endZInvW = endClip.z / endClip.w;

		float stepInvW = (endInvW - startInvW) / steps;
		float stepZInvW = (endZInvW - startZInvW) / steps;

		float shadowFactor = 1.0;
		vec2 currentUV = startUV;
		float currentInvW = startInvW;
		float currentZInvW = startZInvW;

		vec2 offsetRight = vec2(1.0 / texRes.x, 0.0);

		for (int s = 0; s < int(steps); ++s) {
			if (currentUV.x < 0.0 || currentUV.x > 1.0 || currentUV.y < 0.0 || currentUV.y > 1.0) break;

			float currentDepthNDC = currentZInvW / currentInvW;
			float rayDepth = currentDepthNDC * 0.5 + 0.5;
			float sampledDepth = texture(depthTexture, currentUV).r;

			float depthRight = texture(depthTexture, currentUV + offsetRight).r;
			float linearRayZ = abs(getViewZ(rayDepth));
			float linearSampledZ = abs(getViewZ(sampledDepth));
			float linearRightZ = abs(getViewZ(depthRight));

			float depthSlope = abs(linearSampledZ - linearRightZ);
			float minThickness = 0.05;
			float maxThickness = 0.8;
			float slopeSensitivity = 15.0;

			float dynamicThickness = clamp(maxThickness - (depthSlope * slopeSensitivity), minThickness, maxThickness);
			float depthDiff = linearRayZ - linearSampledZ;

			if (depthDiff > 0.001 && depthDiff < dynamicThickness) {
				shadowFactor = 0.4;
				break;
			}

			currentUV += stepUV;
			currentInvW += stepInvW;
			currentZInvW += stepZInvW;
		}

		if (shadowFactor < 1.0) {
			combinedShadow += vec3(0, 1, 0) * (1.0 - shadowFactor);
		}
	}

	vec3 color = texture(sceneTexture, TexCoords).rgb;
	FragColor = vec4(color + combinedShadow, 1.0);
}