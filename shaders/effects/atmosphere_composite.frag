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
	float w = clamp(distKM / 32.0, 0.0, 1.0); // Linear mapping for AP LUT

	return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).rgb;
}

float sampleAerialPerspectiveTransmittance(vec3 rd, float distKM) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float v = elevation / PI + 0.5;
	float w = clamp(distKM / 32.0, 0.0, 1.0);

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

	// 1. Bilateral upsample of low-res clouds and cloud depth
	float sceneDist = dist;
	vec4  cloudData = vec4(0.0);
	float upsampledCloudDist = 0.0;
	float totalWeight = 0.0;

	vec2 lowResUV = TexCoords / cloudTexelSize - 0.5;
	vec2 baseTexel = floor(lowResUV);
	vec2 frac_ = lowResUV - baseTexel;

	for (int dy = 0; dy <= 1; dy++) {
		for (int dx = 0; dx <= 1; dx++) {
			vec2 sampleUV = (baseTexel + vec2(dx, dy) + 0.5) * cloudTexelSize;
			sampleUV = clamp(sampleUV, cloudTexelSize * 0.5, 1.0 - cloudTexelSize * 0.5);

			float sampleCloudDist = texture(cloudDepthTexture, sampleUV).r;

			// Bilinear weight
			float bx = (dx == 0) ? (1.0 - frac_.x) : frac_.x;
			float by = (dy == 0) ? (1.0 - frac_.y) : frac_.y;
			float spatialW = bx * by;

			// Scene depth awareness: avoid bleeding clouds over foreground objects.
			float depthDiff = max(0.0, sampleCloudDist - sceneDist);
			float sceneWeight = exp(-depthDiff / (500.0 * worldScale));

			float w = spatialW * sceneWeight;
			cloudData += texture(cloudTexture, sampleUV) * w;
			upsampledCloudDist += sampleCloudDist * w;
			totalWeight += w;
		}
	}

	if (totalWeight > 1e-4) {
		cloudData /= totalWeight;
		upsampledCloudDist /= totalWeight;
	} else {
		// Fallback for occluded pixels: Clear sky behavior
		cloudData = vec4(0.0, 0.0, 0.0, 1.0);
		upsampledCloudDist = 50000.0 * worldScale;
	}

	vec3  cloudColor = cloudData.rgb;
	float cloudTransmittance = cloudData.a;

	// 2. High-res Atmosphere (Haze/Fog) for the terrain/scene
	float distKM = (dist / 1000.0);
	vec3  inScattering = sampleAerialPerspective(rayDir, distKM);
	float transmittance = sampleAerialPerspectiveTransmittance(rayDir, distKM);

	// 3. Cloud Atmospheric Integration
	// Clouds should also be affected by the atmosphere between them and the camera.
	// We use the upsampled cloud distance for consistency, clamped to the scene depth.
	float effectiveCloudDist = min(upsampledCloudDist, dist);
	float cloudDistKM = (effectiveCloudDist / 1000.0);

	vec3  atmosInScattering = sampleAerialPerspective(rayDir, cloudDistKM);
	float atmosTransmittance = sampleAerialPerspectiveTransmittance(rayDir, cloudDistKM);

	// Combine everything
	bool isSky = depth > 0.9999;

	vec3 result;
	if (!isSky) {
		// Applying shadows to in-scattering (prevent sunrise/sunset glow through hills)
		// but softened to maintain ambient levels.
		float atmosShadow = 1.0;
		if (num_lights > 0 && lights[0].type == LIGHT_TYPE_DIRECTIONAL) {
			vec3 N = texture(normalTexture, TexCoords).xyz * 2.0 - 1.0;
			vec3 L = normalize(-lights[0].direction);
			atmosShadow = calculateShadow(0, worldPos, N, L);
			atmosShadow = mix(0.01, 1.0, atmosShadow);
		}

		// Shadows only apply to the background atmosphere components
		inScattering *= atmosShadow;

		// Physically-based additive blending:
		// result = (Background * CloudTransmittance + CloudScattering) * FrontTransmittance + FrontScattering
		// where Front is the atmosphere between camera and cloud.

		// terrainAtmos = sceneColor * transmittance + inScattering
		// We need to extract the atmosphere behind the cloud.
		vec3 terrainAtmos = sceneColor * transmittance + inScattering;

		// Subtract front fog contribution from the total integrated fog.
		// Note that atmosInScattering is NOT shadowed here as it's the air in front of the cloud.
		result = (terrainAtmos - atmosInScattering) * cloudTransmittance + (cloudColor * atmosTransmittance + atmosInScattering);
		result = max(result, vec3(0.0));
	} else {
		// Sky and colossal objects: Background (Sky) * CloudTransmittance + CloudInScattering
		result = sceneColor * cloudTransmittance + (cloudColor * atmosTransmittance + atmosInScattering);
	}

	FragColor = vec4(result, 1.0);
}
