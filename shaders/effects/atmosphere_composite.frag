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
uniform int   uFrameIndex;

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

// 1. Aligned Joint Bilateral Upsample (3x3 High-Fidelity)
	float sceneDist = dist;
	vec3  sumScattering = vec3(0.0);
	float sumTransmittance = 0.0;
	float sumWeight = 0.0;

	float upsampledCloudDist = 0.0;
	float totalDepthWeight = 0.0;

	// Correctly locate the exact continuous texel position and its integer center
	vec2 lowResUV = TexCoords / cloudTexelSize;
	vec2 nearestTexelCenter = floor(lowResUV) + 0.5;
	float depthTolerance = 100.0 * WORLD_SCALE_VALUE;

	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			vec2 offset = vec2(dx, dy);
			vec2 sampleUV = (nearestTexelCenter + offset) * cloudTexelSize;

			// Aligned coordinate vector from continuous position to the exact sample center
			vec2 pixelOffset = lowResUV - (nearestTexelCenter + offset);

			// Tightened sigma (/0.75) to preserve sharp structural details
			float spatialW = exp(-dot(pixelOffset, pixelOffset) / 0.75);

			vec4 sDepthData = texture(cloudDepthTexture, sampleUV);
			float sDepth = sDepthData.r;
			float sMaxDensity = sDepthData.a;

			vec4 sColor = texture(cloudTexture, sampleUV);
			float structuralRigidity = smoothstep(0.015, 0.05, sMaxDensity);



			// Inside the Composite Upsample (inside the nested dx/dy loop)
			ivec2 sampleCoord = ivec2((nearestTexelCenter + offset));
			float sPhase = bayer4x4StepPhase(sampleCoord, uFrameIndex);
			float rayStepLength = sDepthData.b;

			// Estimate the absolute front of the volume before the dither offset pushed the ray inward
			float continuousFrontEdge = sDepth - (sPhase * rayStepLength);

			// 2. Depth Occlusion Penalty against the high-res scene distance
			// Use the estimated continuous edge, not the quantized sDepth
			float depthDiff = max(0.0, continuousFrontEdge - sceneDist);

			// The tolerance only needs to account for sub-step geometric variation now
			float depthTolerance = rayStepLength * 0.75;
			float depthPenalty = exp(-(depthDiff * depthDiff) / (depthTolerance * depthTolerance));
			depthPenalty = mix(1.0, depthPenalty, structuralRigidity);




			// One-sided depth occlusion penalty
			// float depthDiff = max(0.0, sDepth - sceneDist);
			// float depthPenalty = exp(-(depthDiff * depthDiff) / (depthTolerance * depthTolerance));
			// depthPenalty = mix(1.0, depthPenalty, structuralRigidity);

			// Compute a unified bilateral weight for normalization
			float bilateralW = spatialW * depthPenalty;

			sumScattering += sColor.rgb * bilateralW;
			sumTransmittance += sColor.a * bilateralW;
			sumWeight += bilateralW;

			// Separate weighting for distance interpolation to keep atmospheric depth valid
			float depthAccumW = bilateralW * (1.0 - sColor.a);
			upsampledCloudDist += sDepth * depthAccumW;
			totalDepthWeight += depthAccumW;
		}
	}

	vec4 cloudData;
	if (sumWeight > 0.01) {
		// Normalize using the bilateral weight to prevent dark halos and edge jaggies
		cloudData.rgb = sumScattering / sumWeight;
		cloudData.a = sumTransmittance / sumWeight;

		upsampledCloudDist = (totalDepthWeight > 0.001) ?
							 (upsampledCloudDist / totalDepthWeight) :
							 (50000.0 * WORLD_SCALE_VALUE);
	} else {
		// If the entire 3x3 neighborhood is completely behind geometry, force clean transparency
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
