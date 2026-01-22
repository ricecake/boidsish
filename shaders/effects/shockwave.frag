#version 420 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform vec2      screenSize;
uniform vec3      cameraPos;
uniform mat4      viewMatrix;
uniform mat4      projMatrix;

// Maximum number of simultaneous shockwaves (must match C++ kMaxShockwaves)
#define MAX_SHOCKWAVES 16

// Shockwave data structure (matches ShockwaveGPUData in C++)
struct ShockwaveData {
	vec4 center_radius;     // xyz = center, w = current_radius
	vec4 params;            // x = intensity, y = ring_width, z = max_radius, w = normalized_age
	vec4 color_unused;      // xyz = color, w = unused
};

// Shockwave UBO (binding point 3)
layout(std140, binding = 3) uniform Shockwaves {
	int           shockwave_count;      // Padded to 16 bytes
	int           _pad1, _pad2, _pad3;
	ShockwaveData shockwaves[MAX_SHOCKWAVES];
};

/**
 * Project a world position to screen-space UV coordinates
 */
vec2 worldToScreen(vec3 worldPos) {
	vec4 clipPos = projMatrix * viewMatrix * vec4(worldPos, 1.0);
	vec3 ndc = clipPos.xyz / clipPos.w;
	return ndc.xy * 0.5 + 0.5;
}

/**
 * Calculate the apparent screen radius of a shockwave
 * This projects the 3D radius to 2D screen space for proper distortion
 */
float getScreenRadius(vec3 center, float worldRadius) {
	// Project center and a point on the edge to screen space
	vec2 centerScreen = worldToScreen(center);
	vec2 edgeScreen = worldToScreen(center + vec3(worldRadius, 0.0, 0.0));

	// Calculate screen-space distance (approximate)
	return length(edgeScreen - centerScreen);
}

/**
 * Calculate shockwave distortion based on screen-space distance
 *
 * The shockwave creates a ring of distortion that:
 * 1. Pushes pixels outward from the projected center
 * 2. Creates a refraction-like visual effect
 * 3. Adds color tinting at the wavefront for visibility
 */
void main() {
	// Early out if no shockwaves (using UBO shockwave_count)
	if (shockwave_count <= 0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

	vec2  totalDistortion = vec2(0.0);
	vec3  totalGlow = vec3(0.0);
	float totalGlowWeight = 0.0;

	for (int i = 0; i < shockwave_count && i < MAX_SHOCKWAVES; ++i) {
		vec3  center = shockwaves[i].center_radius.xyz;
		float currentRadius = shockwaves[i].center_radius.w;
		float intensity = shockwaves[i].params.x;
		float ringWidth = shockwaves[i].params.y;
		float maxRadius = shockwaves[i].params.z;
		float age = shockwaves[i].params.w;
		vec3  waveColor = shockwaves[i].color_unused.xyz;

		// Skip if radius is too small
		if (currentRadius < 0.01) continue;

		// Project shockwave center to screen space
		vec4 clipCenter = projMatrix * viewMatrix * vec4(center, 1.0);

		// Skip if behind camera
		if (clipCenter.w <= 0.0) continue;

		vec2 screenCenter = (clipCenter.xy / clipCenter.w) * 0.5 + 0.5;

		// Calculate approximate screen-space radius
		// Account for perspective by using distance to camera
		float distToCamera = length(center - cameraPos);
		float screenRadius = (currentRadius / distToCamera) * 0.8;  // Perspective scaling factor
		float screenRingWidth = (ringWidth / distToCamera) * 0.8;

		// Calculate screen-space distance from center
		vec2 toPixel = TexCoords - screenCenter;
		float pixelDist = length(toPixel);

		// Calculate how close we are to the wavefront ring
		float distFromRing = abs(pixelDist - screenRadius);

		// Gaussian falloff for ring width - creates smooth distortion band
		float ringFactor = exp(-distFromRing * distFromRing / (2.0 * screenRingWidth * screenRingWidth));

		// Skip if outside the ring's influence
		if (ringFactor < 0.001) continue;

		// Calculate distortion direction (radially outward from center)
		vec2 distortDir = pixelDist > 0.001 ? normalize(toPixel) : vec2(0.0);

		// The distortion should create a "lens" or "pressure wave" effect
		// Positive distortion on the leading edge (push outward)
		// Slight negative distortion on the trailing edge (pull back)
		float inside = smoothstep(screenRadius - screenRingWidth, screenRadius, pixelDist);
		float outside = smoothstep(screenRadius, screenRadius + screenRingWidth, pixelDist);

		// Create an asymmetric distortion profile for realistic pressure wave
		float distortProfile;
		if (pixelDist < screenRadius) {
			// Inside the ring - slight inward pull
			distortProfile = -0.3 * (1.0 - inside);
		} else {
			// Outside the ring - strong outward push
			distortProfile = 1.0 * (1.0 - outside);
		}

		// Combine factors for final distortion strength
		float distortStrength = ringFactor * intensity * distortProfile * 0.05;

		totalDistortion += distortDir * distortStrength;

		// Add glow at the wavefront
		// Creates a visible bright ring that's characteristic of shockwaves
		float glowIntensity = ringFactor * intensity * (1.0 - age * age);
		float edgeGlow = exp(-distFromRing * distFromRing / (screenRingWidth * screenRingWidth * 0.5));

		totalGlow += waveColor * glowIntensity * edgeGlow * 0.6;
		totalGlowWeight += glowIntensity * edgeGlow;
	}

	// Apply distortion to UV coordinates
	vec2 distortedUV = TexCoords + totalDistortion;

	// Clamp to prevent sampling outside texture
	distortedUV = clamp(distortedUV, vec2(0.001), vec2(0.999));

	// Sample scene with distorted coordinates
	vec3 sceneColor = texture(sceneTexture, distortedUV).rgb;

	// Add chromatic aberration at strong distortion points
	// This creates the characteristic "heat shimmer" look
	float aberrationStrength = length(totalDistortion) * 15.0;
	if (aberrationStrength > 0.01) {
		vec2 aberrationDir = totalDistortion / (length(totalDistortion) + 0.001);
		float r = texture(sceneTexture, distortedUV + aberrationDir * 0.003).r;
		float b = texture(sceneTexture, distortedUV - aberrationDir * 0.003).b;
		float mixFactor = min(aberrationStrength, 0.7);
		sceneColor.r = mix(sceneColor.r, r, mixFactor);
		sceneColor.b = mix(sceneColor.b, b, mixFactor);
	}

	// Add the glowing wavefront
	// This makes the shockwave visible even in flat-colored areas
	vec3 finalColor = sceneColor;
	if (totalGlowWeight > 0.001) {
		// Normalize glow contribution
		vec3 normalizedGlow = totalGlow / (totalGlowWeight + 0.001);

		// Apply as additive glow with soft falloff
		float glowAmount = min(totalGlowWeight * 0.5, 0.4);
		finalColor += normalizedGlow * glowAmount;

		// Add a subtle white hot-spot at the brightest parts
		float hotSpot = totalGlowWeight * totalGlowWeight * 0.3;
		finalColor += vec3(1.0, 0.95, 0.8) * min(hotSpot, 0.15);
	}

	FragColor = vec4(finalColor, 1.0);
}
