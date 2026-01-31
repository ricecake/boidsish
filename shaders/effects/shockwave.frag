#version 420 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform vec2      screenSize;
uniform vec3      cameraPos;
uniform mat4      viewMatrix;
uniform mat4      projMatrix;
uniform float     nearPlane;
uniform float     farPlane;

// Maximum number of simultaneous shockwaves (must match C++ kMaxShockwaves)
#define MAX_SHOCKWAVES 16

// Shockwave data structure (matches ShockwaveGPUData in C++)
struct ShockwaveData {
	vec4 center_radius; // xyz = center, w = current_radius
	vec4 normal_unused; // xyz = normal, w = unused
	vec4 params;        // x = intensity, y = ring_width, z = max_radius, w = normalized_age
	vec4 color_unused;  // xyz = color, w = unused
};

// Shockwave UBO (binding point 4)
layout(std140, binding = 4) uniform Shockwaves {
	int           shockwave_count; // Padded to 16 bytes
	int           _pad1, _pad2, _pad3;
	ShockwaveData shockwaves[MAX_SHOCKWAVES];
};

/**
 * Linearize depth from the depth buffer
 * Converts from non-linear depth buffer value to linear view-space distance
 */
float linearizeDepth(float depth) {
	float z = depth * 2.0 - 1.0; // Back to NDC
	return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

/**
 * Calculate a view ray in world space from the camera through the current fragment
 */
vec3 getViewRay(vec2 texCoords) {
	// Convert texture coordinates to NDC
	vec2 ndc = texCoords * 2.0 - 1.0;

	// Convert NDC to clip space
	vec4 clip = vec4(ndc, -1.0, 1.0); // -1.0 z for forward direction

	// Convert clip space to view space
	vec4 view = inverse(projMatrix) * clip;
	view = vec4(view.xy, -1.0, 0.0); // Ensure z is forward, ignore translation

	// Convert view space to world space
	vec3 worldDir = (inverse(viewMatrix) * view).xyz;
	return normalize(worldDir);
}

/**
 * Reconstruct world position from depth buffer
 */
vec3 reconstructWorldPos(vec2 texCoords, float depth) {
	// Convert to NDC
	vec2  ndc = texCoords * 2.0 - 1.0;
	float z_ndc = depth * 2.0 - 1.0;

	// Reconstruct clip space position
	vec4 clipPos = vec4(ndc, z_ndc, 1.0);

	// Transform to view space
	vec4 viewPos = inverse(projMatrix) * clipPos;
	viewPos /= viewPos.w;

	// Transform to world space
	vec4 worldPos = inverse(viewMatrix) * viewPos;
	return worldPos.xyz;
}

/**
 * Calculate intersection of a ray with a plane
 * Returns distance along the ray, or -1.0 if no intersection
 */
float rayPlaneIntersect(vec3 rayOrigin, vec3 rayDir, vec3 planeCenter, vec3 planeNormal) {
	float denom = dot(planeNormal, rayDir);
	if (abs(denom) > 0.0001) {
		float t = dot(planeCenter - rayOrigin, planeNormal) / denom;
		if (t >= 0.0) { // Intersection must be in front of the camera
			return t;
		}
	}
	return -1.0;
}

/**
 * Calculate shockwave distortion based on world-space intersection
 *
 * The shockwave creates a disc of distortion in world space that:
 * 1. Pushes pixels outward from the projected center
 * 2. Creates a refraction-like visual effect
 * 3. Adds color tinting at the wavefront for visibility
 * 4. Is occluded by scene geometry (respects depth buffer)
 * 5. Wraps around scene geometry instead of passing through it
 */
void main() {
	// Early out if no shockwaves (using UBO shockwave_count)
	if (shockwave_count <= 0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

	// Get scene depth at this pixel and convert to linear distance
	float sceneDepth = texture(depthTexture, TexCoords).r;
	float sceneLinearDepth = linearizeDepth(sceneDepth);

	// Reconstruct world position of scene geometry at this pixel
	vec3 sceneWorldPos = reconstructWorldPos(TexCoords, sceneDepth);

	vec2  totalDistortion = vec2(0.0);
	vec3  totalGlow = vec3(0.0);
	float totalGlowWeight = 0.0;

	// Get the view ray for this fragment
	vec3 viewRay = getViewRay(TexCoords);

	for (int i = 0; i < shockwave_count && i < MAX_SHOCKWAVES; ++i) {
		vec3  center = shockwaves[i].center_radius.xyz;
		vec3  normal = shockwaves[i].normal_unused.xyz;
		float currentRadius = shockwaves[i].center_radius.w;
		float intensity = shockwaves[i].params.x;
		float ringWidth = shockwaves[i].params.y;
		float maxRadius = shockwaves[i].params.z;
		float age = shockwaves[i].params.w;
		vec3  waveColor = shockwaves[i].color_unused.xyz;

		// Skip if radius is too small or normal is invalid
		if (currentRadius < 0.01 || length(normal) < 0.1)
			continue;

		// Find where the view ray intersects the shockwave's plane
		float t = rayPlaneIntersect(cameraPos, viewRay, center, normal);
		if (t < 0.0)
			continue; // No intersection in front of camera

		// Calculate world-space intersection point on the shockwave plane
		vec3 planeIntersectPos = cameraPos + viewRay * t;

		// Calculate distance from shockwave center on the plane
		float planeWorldDist = length(planeIntersectPos - center);

		// Calculate distance from scene geometry to shockwave center
		// This is the actual 3D distance, used for "wrapping" effect
		float sceneDistFromCenter = length(sceneWorldPos - center);

		// Project scene geometry onto the shockwave plane to get planar distance
		vec3  sceneToCenter = sceneWorldPos - center;
		float sceneDistAlongNormal = dot(sceneToCenter, normal);
		vec3  sceneProjectedOnPlane = sceneWorldPos - sceneDistAlongNormal * normal;
		float scenePlanarDist = length(sceneProjectedOnPlane - center);

		// Determine which distance to use for the ring effect:
		// - If the plane intersection is in front of geometry, use the plane
		// - If geometry is in front of the plane, the shockwave "wraps" onto geometry
		float effectiveDist;
		bool  showOnGeometry = false;

		if (t > sceneLinearDepth) {
			// Scene geometry is in front of the shockwave plane
			// Check if the geometry is close enough to the plane to show the effect on it
			// (shockwave wraps around obstacles)
			if (abs(sceneDistAlongNormal) < currentRadius * 1.5) {
				// Use the scene geometry's planar distance - the shockwave wraps onto it
				effectiveDist = scenePlanarDist;
				showOnGeometry = true;
			} else {
				// Geometry is too far from the plane, skip this shockwave for this pixel
				continue;
			}
		} else {
			// Shockwave plane is in front - use plane intersection
			effectiveDist = planeWorldDist;
		}

		// Skip if outside the shockwave's max radius
		if (effectiveDist > maxRadius)
			continue;

		// Calculate how close we are to the wavefront ring
		float distFromRing = abs(effectiveDist - currentRadius);

		// Gaussian falloff for ring width - creates smooth distortion band
		float ringFactor = exp(-distFromRing * distFromRing / (2.0 * ringWidth * ringWidth));

		// Skip if outside the ring's influence
		if (ringFactor < 0.001)
			continue;

		// Choose the appropriate position for screen-space calculations
		vec3 effectPos = showOnGeometry ? sceneWorldPos : planeIntersectPos;

		// Project the effect position to screen space for distortion direction
		vec4 clipPos = projMatrix * viewMatrix * vec4(effectPos, 1.0);
		vec2 screenPos = (clipPos.xy / clipPos.w) * 0.5 + 0.5;

		// The direction of distortion should be from the screen projection
		// of the shockwave center towards the current fragment.
		vec4 clipCenter = projMatrix * viewMatrix * vec4(center, 1.0);
		if (clipCenter.w <= 0.0)
			continue; // Should be rare due to plane intersection
		vec2 screenCenter = (clipCenter.xy / clipCenter.w) * 0.5 + 0.5;

		vec2 distortDir = TexCoords - screenCenter;
		if (length(distortDir) > 0.001) {
			distortDir = normalize(distortDir);
		} else {
			// If we're at the center, pick an arbitrary direction
			distortDir = normalize(TexCoords - vec2(0.5));
		}

		// The distortion should create a "lens" or "pressure wave" effect
		float inside = smoothstep(currentRadius - ringWidth, currentRadius, effectiveDist);
		float outside = smoothstep(currentRadius, currentRadius + ringWidth, effectiveDist);

		// Create an asymmetric distortion profile for realistic pressure wave
		float distortProfile;
		if (effectiveDist < currentRadius) {
			// Inside the ring - slight inward pull
			distortProfile = -0.3 * (1.0 - inside);
		} else {
			// Outside the ring - strong outward push
			distortProfile = 1.0 * (1.0 - outside);
		}

		// Combine factors for final distortion strength
		// Reduce distortion effect further away from the camera
		float distToCamera = length(center - cameraPos);
		float perspectiveFactor = 1.0 / (1.0 + distToCamera * 0.1);
		float distortStrength = ringFactor * intensity * distortProfile * 0.05 * perspectiveFactor;

		totalDistortion += distortDir * distortStrength;

		// Add glow at the wavefront
		// Creates a visible bright ring that's characteristic of shockwaves
		float glowIntensity = ringFactor * intensity * (1.0 - age * age);
		float edgeGlow = exp(-distFromRing * distFromRing / (ringWidth * ringWidth * 0.5));

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
		vec2  aberrationDir = totalDistortion / (length(totalDistortion) + 0.001);
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
