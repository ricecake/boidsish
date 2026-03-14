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

	// Tile-based optimization: check if this fragment needs screen space shadows
	// Project to first light's shadow space
	if (numShadowLights > 0) {
		vec4 lightSpacePos = lightSpaceMatrices[0] * vec4(worldPos, 1.0);
		vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
		projCoords = projCoords * 0.5 + 0.5;

		if (projCoords.x >= 0.0 && projCoords.x <= 1.0 && projCoords.y >= 0.0 && projCoords.y <= 1.0) {
			uint maskValue = texture(sssTileMask, vec3(projCoords.xy, 0.0)).r;
			if (maskValue == 0) {
				FragColor = texture(sceneTexture, TexCoords);
				return;
			}
		}
	}

	vec3 lightDir;
	if (lights[0].type == 1) {
		lightDir = normalize(-lights[0].direction);
	} else {
		lightDir = normalize(lights[0].position - worldPos);
	}

	// 1. Setup Vector Endpoints (Executed once)
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

	// 2. DDA Step Calculation
	vec2 deltaUV = endUV - startUV;
	vec2 texRes = vec2(textureSize(depthTexture, 0));
	vec2 pixelDistance = abs(deltaUV * texRes);

	float steps = max(pixelDistance.x, pixelDistance.y);
	steps = min(steps, 128.0); // Safety cap to prevent GPU TDR

	if (steps < 1.0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

	// 3. Perspective-Correct Interpolators
	vec2 stepUV = deltaUV / steps;

	float startInvW = 1.0 / startClip.w;
	float endInvW = 1.0 / endClip.w;
	float startZInvW = startClip.z / startClip.w;
	float endZInvW = endClip.z / endClip.w;

	float stepInvW = (endInvW - startInvW) / steps;
	float stepZInvW = (endZInvW - startZInvW) / steps;

	float shadow = 1.0;
	float jitter = 0;//fastBlueNoise(TexCoords);

	vec2 currentUV = startUV + (stepUV * jitter);
	float currentInvW = startInvW + (stepInvW * jitter);
	float currentZInvW = startZInvW + (stepZInvW * jitter);

	vec2 offsetRight = vec2(1.0 / texRes.x, 0.0);

	// 4. The Marching Loop
	for (int i = 0; i < int(steps); ++i) {
		if (currentUV.x < 0.0 || currentUV.x > 1.0 || currentUV.y < 0.0 || currentUV.y > 1.0) break;

		float currentDepthNDC = currentZInvW / currentInvW;
		float rayDepth = currentDepthNDC * 0.5 + 0.5;

		float sampledDepth = texture(depthTexture, currentUV).r;

		// 5. Dynamic Thickness via Depth Gradient
		float depthRight = texture(depthTexture, currentUV + offsetRight).r;

		// Convert everything to linear world-space units for consistent comparison
		float linearRayZ = abs(getViewZ(rayDepth));
		float linearSampledZ = abs(getViewZ(sampledDepth));
		float linearRightZ = abs(getViewZ(depthRight));

		float depthSlope = abs(linearSampledZ - linearRightZ);

		// Map the slope to a thickness.
		// Flat surfaces facing the camera (low slope) = thick
		// Edges curving away (high slope) = thin
		// *NOTE: You will need to tune these 3 variables to your world scale*
		float minThickness = 0.05;
		float maxThickness = 0.8;
		float slopeSensitivity = 15.0;

		float dynamicThickness = clamp(maxThickness - (depthSlope * slopeSensitivity), minThickness, maxThickness);

		float depthDiff = linearRayZ - linearSampledZ;

		if (depthDiff > 0.001 && depthDiff < dynamicThickness) {
			shadow = 0.4;
			break;
		}

		currentUV += stepUV;
		currentInvW += stepInvW;
		currentZInvW += stepZInvW;
	}

	vec3 color = texture(sceneTexture, TexCoords).rgb;
	FragColor = vec4(color + vec3(0,1,0)* shadow, 1.0);
}