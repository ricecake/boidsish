#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D cloudTexture; // Unified volumetric result
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

	vec4  volumetricData = texture(cloudTexture, TexCoords);

	// The unified volumetric pass already computes scattering and transmittance from camera to scene depth.
	// However, it's done at lower resolution and then upsampled.
	// We combine it with the aerial perspective LUT for the VERY far atmosphere if needed,
	// but for now let's assume the unified pass covers it.

	vec3  volScattering = volumetricData.rgb;
	float volTransmittance = volumetricData.a;

	// In the unified pass, we integrate along the full ray.
	// result = sceneColor * volTransmittance + volScattering
	// BUT sceneColor usually already has some lighting.
	// If the scene color is the raw lit surface (before atmosphere), then this is correct.
	// If sceneColor already includes global aerial perspective, we might be double-counting.

	// Actually, the renderer usually applies lighting but not global atmosphere before post-processing.
	// Let's check how other effects are applied.

	vec3 result = sceneColor * volTransmittance + volScattering;

	FragColor = vec4(result, 1.0);
}
