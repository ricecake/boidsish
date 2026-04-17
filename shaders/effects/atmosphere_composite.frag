#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D cloudTexture; // Low-res clouds (temporally accumulated)

uniform mat4 invView;
uniform mat4 invProjection;

uniform float hazeDensity;
uniform float hazeHeight;
uniform vec3  hazeColor;

uniform vec2 cloudTexelSize; // 1.0 / lowResSize

// u_transmittanceLUT is declared in helpers/lighting.glsl
uniform sampler3D u_aerialPerspectiveLUT;

#include "../atmosphere/common.glsl"
#include "../helpers/lighting.glsl"
#include "helpers/math.glsl"
#include "../helpers/volumetric_lighting.glsl"

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
	float planarDepth = dot(worldPos - viewPos, viewDir);

	if (depth >= 0.99999) {
		dist = 50000.0 * worldScale;
		planarDepth = 50000.0 * worldScale;
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

	// 1.5 Volumetric Lighting (Cascaded Froxel Grid)
	vec4 volLight = getVolumetricLighting(TexCoords, planarDepth);
	vec3 volScattering = volLight.rgb;
	float volTransmittance = volLight.a;

	// 2. High-res Atmosphere (Haze)
	float distKM = (dist / 1000.0) * hazeDensity;
	vec3  inScattering = sampleAerialPerspective(rayDir, distKM);
	float transmittance = sampleAerialPerspectiveTransmittance(rayDir, distKM);

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
		// Apply Volumetrics to scene first
		vec3 baseColor = sceneColor * volTransmittance + volScattering;

		// Terrain/objects: apply aerial perspective and clouds
		vec3 terrainAtmos = baseColor * transmittance + inScattering;
		vec3 cloudsAtmos = cloudColor * atmosTransmittance + atmosInScattering * (1.0 - cloudTransmittance);
		result = mix(cloudsAtmos, terrainAtmos, cloudTransmittance);
	} else {
		// Sky and colossal objects: preserve scene output (sun, moon, stars, colossal)
		// but volumetrics still apply to the distant view (shafts of light)
		vec3 baseColor = sceneColor * volTransmittance + volScattering;

		vec3 cloudsAtmos = cloudColor * atmosTransmittance + atmosInScattering * (1.0 - cloudTransmittance);
		result = baseColor * cloudTransmittance + cloudsAtmos;
	}

	FragColor = vec4(result, 1.0);
}
