#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D cloudTexture; // Low-res clouds (temporally accumulated)
layout(binding = 3) uniform sampler2D normalTexture;
layout(binding = 4) uniform sampler2D cloudDepthTexture;

uniform mat4 invView;
uniform mat4 invProjection;

uniform vec2 cloudTexelSize; // 1.0 / lowResSize

// u_transmittanceLUT is declared in helpers/lighting.glsl
uniform sampler3D u_aerialPerspectiveLUT;

#define USE_TERRAIN_DATA
#include "../atmosphere/common.glsl"
#include "../helpers/terrain_shadows.glsl"
#include "../helpers/lighting.glsl"
#include "helpers/math.glsl"

vec3 sampleAerialPerspective(vec3 rd, float distKM) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float v = elevation / PI + 0.5;
	float w = (distKM / 32.0); // maxDist in AP LUT

	return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).rgb;
}

float sampleAerialPerspectiveTransmittance(vec3 rd, float distKM) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float v = elevation / PI + 0.5;
	float w = (distKM / 32.0);

	return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).a;
}

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;

	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3 worldPos = (invView * viewSpacePosition).xyz;

	vec3  rayDir = normalize(worldPos - viewPos);
	float dist = length(worldPos - viewPos);

	if (depth > 0.999999) {
		dist = 50000.0 * worldScale;
	}

	// 1. Bilateral upsample of low-res clouds
	// Weight nearby low-res texels by cloud depth similarity to avoid bleeding across cloud edges
	float sceneDist = dist;
	vec4  cloudData = vec4(0.0);
	float totalWeight = 0.0;

	// Sample a 4x4 neighborhood for better upsampling quality near edges
	vec2 lowResUV = TexCoords / cloudTexelSize - 0.5;
	vec2 baseTexel = floor(lowResUV);
	vec2 frac_ = lowResUV - baseTexel;

	for (int dy = -1; dy <= 2; dy++) {
		for (int dx = -1; dx <= 2; dx++) {
			vec2 sampleUV = (baseTexel + vec2(dx, dy) + 0.5) * cloudTexelSize;
			sampleUV = clamp(sampleUV, cloudTexelSize * 0.5, 1.0 - cloudTexelSize * 0.5);

			float sampleCloudDist = texture(cloudDepthTexture, sampleUV).r;

			// Reconstruct high-res scene depth at this low-res texel is NOT what we want.
			// We want to compare the high-res scene depth with the low-res cloud depth
			// AND the low-res cloud depth at neighbor with current low-res cloud depth.

			// Bilinear weight
			float bx = (dx == -1) ? max(0.0, 1.0 - (frac_.x + 1.0)) :
			           (dx == 0)  ? (1.0 - frac_.x) :
			           (dx == 1)  ? frac_.x :
			           max(0.0, frac_.x - 1.0);
			float by = (dy == -1) ? max(0.0, 1.0 - (frac_.y + 1.0)) :
			           (dy == 0)  ? (1.0 - frac_.y) :
			           (dy == 1)  ? frac_.y :
			           max(0.0, frac_.y - 1.0);
			float spatialW = bx * by;

			// Cloud depth similarity weight
			// This helps maintain sharp cloud silhouettes
			float centerCloudDist = texture(cloudDepthTexture, TexCoords).r;
			float cloudDepthDiff = abs(centerCloudDist - sampleCloudDist) / max(centerCloudDist, 1.0);
			float cloudDepthW = exp(-cloudDepthDiff * 20.0);

			// Scene depth awareness: avoid bleeding clouds over foreground objects
			// If the scene is closer than the cloud, the cloud contribution should be minimized
			float sceneWeight = 1.0;
			if (sceneDist < sampleCloudDist - 10.0 * worldScale) {
				sceneWeight = 0.01;
			}

			float w = spatialW * cloudDepthW * sceneWeight;
			cloudData += texture(cloudTexture, sampleUV) * w;
			totalWeight += w;
		}
	}
	cloudData /= max(totalWeight, 1e-6);

	vec3  cloudColor = cloudData.rgb;
	float cloudTransmittance = cloudData.a;

	// 2. High-res Atmosphere (Haze)
	float distKM = (dist / 1000.0);
	vec3  inScattering = sampleAerialPerspective(rayDir, distKM);
	float transmittance = sampleAerialPerspectiveTransmittance(rayDir, distKM);

	// 3. Cloud Atmospheric Integration
	// Clouds should also be affected by the atmosphere between them and the camera.
	float cloudDist = (cloudAltitude * worldScale - viewPos.y) / max(abs(rayDir.y), 0.01);
	cloudDist = clamp(cloudDist, 0.0, dist);
	float cloudDistKM = (cloudDist / 1000.0);

	vec3  atmosInScattering = sampleAerialPerspective(rayDir, cloudDistKM);
	float atmosTransmittance = sampleAerialPerspectiveTransmittance(rayDir, cloudDistKM);

	// Combine everything
	// Colossal objects write depth ~0.99999 — treat them like sky (no aerial perspective
	// fog, which would completely wash them out at that reconstructed distance)
	bool isSky = depth > 0.9999;

	vec3 result;
	if (!isSky) {
		// 4. Shadowing for in-scattering (prevent sunrise/sunset glow through hills)
		float atmosShadow = 1.0;
		if (num_lights > 0 && lights[0].type == LIGHT_TYPE_DIRECTIONAL) {
			vec3 N = texture(normalTexture, TexCoords).xyz * 2.0 - 1.0;
			vec3 L = normalize(-lights[0].direction);
			atmosShadow = calculateShadow(0, worldPos, N, L);

			// Soften the shadow for atmosphere — don't make it pitch black,
			// atmosphere still has ambient light.
			atmosShadow = mix(0.1, 1.0, atmosShadow);
		}

		inScattering *= atmosShadow;
		atmosInScattering *= atmosShadow;

		// Terrain/objects: apply aerial perspective and clouds
		vec3 terrainAtmos = sceneColor * transmittance + inScattering;
		vec3 cloudsAtmos = cloudColor * atmosTransmittance + atmosInScattering * (1.0 - cloudTransmittance);
		result = mix(cloudsAtmos, terrainAtmos, cloudTransmittance);
	} else {
		// Sky and colossal objects: preserve scene output (sun, moon, stars, colossal)
		// and blend clouds on top
		vec3 cloudsAtmos = cloudColor * atmosTransmittance + atmosInScattering * (1.0 - cloudTransmittance);
		result = sceneColor * cloudTransmittance + cloudsAtmos;
	}

	FragColor = vec4(result, 1.0);
}
