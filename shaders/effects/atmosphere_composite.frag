#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D cloudTexture; // Low-res clouds (temporally accumulated)

uniform mat4 invView;
uniform mat4 invProjection;

uniform float hazeDensity;
uniform float hazeHeight;
uniform vec3  hazeColor;

uniform vec2 cloudTexelSize; // 1.0 / lowResSize

// u_transmittanceLUT is declared in helpers/lighting.glsl
layout(binding = [[ATMOSPHERE_AERIAL_PERSPECTIVE_BINDING]]) uniform sampler3D u_aerialPerspectiveLUT;

layout(binding = [[VOLUMETRIC_CASCADE0_BINDING]]) uniform sampler3D u_volumetricCascade0;
layout(binding = [[VOLUMETRIC_CASCADE1_BINDING]]) uniform sampler3D u_volumetricCascade1;

struct VolumetricLighting {
    vec4 cascadeRanges;
    ivec4 cascadeRes;
    float intensity;
    float scatteringCoeff;
    float extinctionCoeff;
    float mieAnisotropy;
    vec4 ambientFactor;
    float phaseG;
};

layout(std140, binding = [[VOLUMETRIC_LIGHTING_BINDING]]) uniform VolumetricUniforms {
    VolumetricLighting u_volData;
};

#include "../atmosphere/common.glsl"
#include "../helpers/lighting.glsl"
#include "helpers/math.glsl"

vec3 sampleAerialPerspective(vec3 rd, float distKM) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float v = elevation / PI + 0.5;
	float w = distKM / 32.0; // maxDist in AP LUT

	return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).rgb;
}

float sampleAerialPerspectiveTransmittance(vec3 rd, float distKM) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float v = elevation / PI + 0.5;
	float w = distKM / 32.0;

	return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).a;
}

uniform sampler3D u_volumetricCascade2;
uniform sampler3D u_volumetricCascade3;

vec4 sampleVolumetricChained(vec2 screenUV, float dist) {
    float ranges[5] = { 0.1, u_volData.cascadeRanges.x, u_volData.cascadeRanges.y, u_volData.cascadeRanges.z, u_volData.cascadeRanges.w };

    vec3 totalScat = vec3(0.0);
    float totalTrans = 1.0;

    // Cascade 0
    if (dist > ranges[0]) {
        float d = min(dist, ranges[1]);
        float w = log(d / ranges[0]) / log(ranges[1] / ranges[0]);
        vec4 sample0 = texture(u_volumetricCascade0, vec3(screenUV, w));
        totalScat += totalTrans * sample0.rgb;
        totalTrans *= sample0.a;
    }
    // Cascade 1
    if (dist > ranges[1]) {
        float d = min(dist, ranges[2]);
        float w = log(d / ranges[1]) / log(ranges[2] / ranges[1]);
        vec4 sample1 = texture(u_volumetricCascade1, vec3(screenUV, w));
        totalScat += totalTrans * sample1.rgb;
        totalTrans *= sample1.a;
    }
    // Cascade 2
    if (dist > ranges[2]) {
        float d = min(dist, ranges[3]);
        float w = log(d / ranges[2]) / log(ranges[3] / ranges[2]);
        vec4 sample2 = texture(u_volumetricCascade2, vec3(screenUV, w));
        totalScat += totalTrans * sample2.rgb;
        totalTrans *= sample2.a;
    }
    // Cascade 3
    if (dist > ranges[3]) {
        float d = min(dist, ranges[4]);
        float w = log(d / ranges[3]) / log(ranges[4] / ranges[3]);
        vec4 sample3 = texture(u_volumetricCascade3, vec3(screenUV, w));
        totalScat += totalTrans * sample3.rgb;
        totalTrans *= sample3.a;
    }

    return vec4(totalScat, totalTrans);
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
	float linearDepth = abs(viewSpacePosition.z);

	if (depth >= 0.99999) {
		dist = 50000.0 * worldScale;
		linearDepth = 50000.0 * worldScale;
	}

	// 1. Bilateral upsample of low-res clouds
	// Weight nearby low-res texels by depth similarity to avoid bleeding across edges
	float centerDepth = dist;
	vec4  cloudData = vec4(0.0);
	float totalWeight = 0.0;

	// Sample a 2x2 neighborhood of the nearest low-res texels
	vec2 lowResUV = TexCoords / cloudTexelSize - 0.5;
	vec2 baseTexel = floor(lowResUV);
	vec2 frac_ = lowResUV - baseTexel;

	for (int dy = 0; dy <= 1; dy++) {
		for (int dx = 0; dx <= 1; dx++) {
			vec2 sampleUV = (baseTexel + vec2(dx, dy) + 0.5) * cloudTexelSize;
			sampleUV = clamp(sampleUV, cloudTexelSize * 0.5, 1.0 - cloudTexelSize * 0.5);

			// Reconstruct depth at this low-res texel center
			float sampleDepthRaw = texture(depthTexture, sampleUV).r;
			float sampleDist;
			if (sampleDepthRaw >= 1.0) {
				sampleDist = 50000.0 * worldScale;
			} else {
				float sz = sampleDepthRaw * 2.0 - 1.0;
				vec4  sClip = vec4(sampleUV * 2.0 - 1.0, sz, 1.0);
				vec4  sView = invProjection * sClip;
				sView /= sView.w;
				vec3 sWorld = (invView * sView).xyz;
				sampleDist = length(sWorld - viewPos);
			}

			// Bilinear weight
			float bx = (dx == 0) ? (1.0 - frac_.x) : frac_.x;
			float by = (dy == 0) ? (1.0 - frac_.y) : frac_.y;
			float spatialW = bx * by;

			// Depth similarity weight — exponential falloff
			float depthDiff = abs(centerDepth - sampleDist) / max(centerDepth, 1.0);
			float depthW = exp(-depthDiff * 150.0);

			float w = spatialW * depthW;
			cloudData += texture(cloudTexture, sampleUV) * w;
			totalWeight += w;
		}
	}
	cloudData /= max(totalWeight, 1e-6);

	vec3  cloudColor = cloudData.rgb;
	float cloudTransmittance = cloudData.a;

	// 2. High-res Atmosphere (Haze)
	float distKM = (dist / 1000.0) * hazeDensity;
	vec3  inScattering = sampleAerialPerspective(rayDir, distKM);
	float transmittance = sampleAerialPerspectiveTransmittance(rayDir, distKM);

    // 2.5 Volumetric Lighting (Froxels)
    // Use linear Z-depth for froxel sampling to match grid distribution
    vec4 volData = sampleVolumetricChained(TexCoords, linearDepth);
    vec3 volScattering = volData.rgb;
    float volTransmittance = volData.a;

    // To prevent terrain bleeding, we need to ensure that the unshadowed high-res haze (Aerial Perspective)
    // is masked by the volumetric transmittance. However, the high-res haze also needs to be
    // consistent with the shadowed froxels.
    // We apply a depth-based weight to the aerial perspective to ensure it doesn't bleed through
    // close terrain.
    float bleedingMask = smoothstep(0.0, 10.0, dist);
    inScattering *= bleedingMask;

    // Apply volumetric lighting
    inScattering = inScattering * volTransmittance + volScattering;
    transmittance *= volTransmittance;

	// 3. Cloud Atmospheric Integration
	// Clouds should also be affected by the atmosphere between them and the camera.
	float cloudDist = (cloudAltitude * worldScale - viewPos.y) / max(abs(rayDir.y), 0.01);
	cloudDist = clamp(cloudDist, 0.0, dist);
	float cloudDistKM = (cloudDist / 1000.0) * hazeDensity;

	vec3  atmosInScattering = sampleAerialPerspective(rayDir, cloudDistKM);
	float atmosTransmittance = sampleAerialPerspectiveTransmittance(rayDir, cloudDistKM);

	// Combine everything
	// Colossal objects write depth ~0.99999 — treat them like sky (no aerial perspective
	// fog, which would completely wash them out at that reconstructed distance)
	bool isSky = depth >= 0.99999;

	vec3 result;
	if (!isSky) {
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
