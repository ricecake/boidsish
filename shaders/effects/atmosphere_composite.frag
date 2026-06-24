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
		dist = 50000.0 * WORLD_SCALE_VALUE;
	}

	// 1. Bilateral upsample of low-res clouds and cloud depth
	float sceneDist = dist;
	vec3  totalScattering = vec3(0.0);
	float totalTransmittance = 0.0;
	float upsampledCloudDist = 0.0;
	float totalWeight = 0.0;

	vec2 lowResUV = TexCoords / cloudTexelSize - 0.5;
	vec2 baseTexel = floor(lowResUV);
	vec2 frac_ = lowResUV - baseTexel;
	float depthTolerance = 10.0 * WORLD_SCALE_VALUE;

	for (int dy = 0; dy <= 1; dy++) {
		for (int dx = 0; dx <= 1; dx++) {
			vec2 sampleUV = (baseTexel + vec2(dx, dy) + 0.5) * cloudTexelSize;

			vec4 sDepthData = texture(cloudDepthTexture, sampleUV);
			float sDepth = sDepthData.r;
			float sMaxDensity = sDepthData.a; // Read the PSD

			vec4 sColor = texture(cloudTexture, sampleUV);
			float w = ((dx == 0) ? (1.0 - frac_.x) : frac_.x) * ((dy == 0) ? (1.0 - frac_.y) : frac_.y);

			// Determine if this sample is a hard structure or soft haze
			float structuralRigidity = smoothstep(0.01, 0.15, sMaxDensity);

			float depthDiff = max(0.0, sDepth - sceneDist);
			float depthWeight = exp(-(depthDiff * depthDiff) / (depthTolerance * depthTolerance));
			depthWeight = mix(1.0, depthWeight, structuralRigidity);
			w *= depthWeight;

			totalScattering += sColor.rgb * w;
			totalTransmittance += sColor.a * w;

			upsampledCloudDist += sDepth * w;
			totalWeight += w;
		}
	}

	vec4 cloudData;
	if (totalWeight > 0.0) {
		cloudData.rgb = totalScattering / totalWeight;
		cloudData.a = totalTransmittance / totalWeight;
		upsampledCloudDist /= totalWeight;
	} else {
		cloudData = vec4(0.0, 0.0, 0.0, 1.0);
		upsampledCloudDist = 50000.0 * WORLD_SCALE_VALUE;
	}
	vec3  cloudScattering = cloudData.rgb;
	float cloudTransmittance = cloudData.a;

	// 2. Atmosphere Integration
	// We need the atmosphere between the camera and the cloud, and between the camera and the scene.
	float sceneDistKM = (dist / 1000.0);
	float cloudDistKM = (min(upsampledCloudDist, dist) / 1000.0);

	vec3  atmosInScattering = sampleAerialPerspective(rayDir, sceneDistKM);
	float atmosTransmittance = sampleAerialPerspectiveTransmittance(rayDir, sceneDistKM);

	vec3  cloudFrontInScattering = sampleAerialPerspective(rayDir, cloudDistKM);
	float cloudFrontTransmittance = sampleAerialPerspectiveTransmittance(rayDir, cloudDistKM);

    // Apply terrain shadows to atmosphere behind clouds
    if (depth < 0.999) {
        float atmosShadow = 1.0;
        if (num_lights > 0 && lights[0].type == LIGHT_TYPE_DIRECTIONAL) {
            vec3 N = texture(normalTexture, TexCoords).xyz * 2.0 - 1.0;
            vec3 L = normalize(-lights[0].direction);
            atmosShadow = calculateShadow(0, worldPos, N, L);
            atmosShadow = mix(0.1, 1.0, atmosShadow);
        }
        // Shadow only the atmosphere that is behind the cloud.
        // We approximate this by shadowing the total in-scattering but keeping the portion in front of the cloud lit.
        atmosInScattering = mix(cloudFrontInScattering, atmosInScattering, atmosShadow);
    }

	// 3. Final Composition
	// result = (Background * CloudTransmittance + CloudScattering) * FrontTransmittance + FrontScattering
    // where Background is the scene behind the cloud atmosphere.

    // The 'sceneColor' already includes the full atmosphere (sceneColor = raw * atmosTrans + atmosInScat).
    // To get the "background behind the cloud", we need to 'undo' the front atmosphere.
    vec3 background = (sceneColor - cloudFrontInScattering);

    // Physically, the cloud layer is inserted into the atmosphere.
    // Result = background * cloudTransmittance + cloudScattering;
    // Then re-apply the front atmosphere:
    // Result = Result * cloudFrontTransmittance + cloudFrontInScattering;

    // However, cloudScattering from the low-res pass ALREADY includes the lighting at that altitude.
    // The correct formula for a volume integrated into atmosphere is:
    // Result = (SceneColor_behind_cloud) * cloudTransmittance + cloudScattering_integrated

    vec3 result = max(sceneColor - cloudFrontInScattering, vec3(0.0)) * cloudTransmittance + (cloudScattering * cloudFrontTransmittance + cloudFrontInScattering);

	FragColor = vec4(result, 1.0);
}
