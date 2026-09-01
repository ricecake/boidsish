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

	if (rayDir.y < 0.0) {
		float t_floor = max(0.0, viewPos.y) / max(-rayDir.y, 0.00001);
		dist = min(dist, t_floor);
	}

	vec4  cloudData = texture(cloudTexture, TexCoords);
	vec4  cloudDepthData = texture(cloudDepthTexture, TexCoords);
	float upsampledCloudDist = cloudDepthData.r;

	vec3  cloudScattering = cloudData.rgb;
	float cloudTransmittance = cloudData.a;

	// Depth occlusion penalty against the scene distance
	float sceneDist = dist;
	if (sceneDist < upsampledCloudDist - (cloudDepthData.b * 2.0)) {
		cloudScattering = vec3(0.0);
		cloudTransmittance = 1.0;
		upsampledCloudDist = 50000.0 * WORLD_SCALE_VALUE;
	}

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
	// Skip blending if rays never hit the cloud shell or have no cloud opacity
	if (cloudTransmittance >= 0.9999 || cloudDepthData.a <= 0.0001) {
		FragColor = vec4(sceneColor, 1.0);
		return;
	}

	vec3 result = max(sceneColor - cloudFrontInScattering, vec3(0.0)) * cloudTransmittance + (cloudScattering * cloudFrontTransmittance + cloudFrontInScattering);

	FragColor = vec4(result, 1.0);
}
