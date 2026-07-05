#version 460 core
out vec4 FragColor;
in vec2 TexCoords;
layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D cloudTexture;
layout(binding = 3) uniform sampler2D normalTexture;
layout(binding = 4) uniform sampler2D cloudDepthTexture;
uniform mat4 invView;
uniform mat4 invProjection;
uniform sampler3D u_aerialPerspectiveLUT;
#define USE_TERRAIN_DATA
#include "../atmosphere/common.glsl"
#include "../helpers/terrain_shadows.glsl"
#include "../helpers/lighting.glsl"
#include "helpers/math.glsl"

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;
	vec4  volData = texture(cloudTexture, TexCoords);
	vec4  volDepth = texture(cloudDepthTexture, TexCoords);

	float z = depth * 2.0 - 1.0;
	vec4  clipPos = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewPos4 = invProjection * clipPos;
	viewPos4 /= viewPos4.w;
	vec3  worldP = (invView * viewPos4).xyz;
	vec3  rd = normalize(worldP - viewPos);
	float dist = length(worldP - viewPos);
	if (depth > 0.999999) dist = 50000.0 * WORLD_SCALE_VALUE;

	// sceneColor should be RAW lit color. If it already has AP, we must undo it.
	// But usually it's better to render scene without AP if we use unified pass.
	// Assume unified pass covers [0, dist]
	vec3 result = sceneColor * volData.a + volData.rgb;

	// If depth is sky, add the distant atmosphere beyond the cloud/far-plane
	if (depth > 0.999) {
		float azimuth = atan(rd.x, -rd.z);
		if (azimuth < 0.0) azimuth += 2.0 * PI;
		float elevation = asin(clamp(rd.y, -1.0, 1.0));
		vec3 sky = texture(u_aerialPerspectiveLUT, vec3(azimuth / (2.0 * PI), elevation / PI + 0.5, 1.0)).rgb;
		result = mix(result, sky, volData.a);
	}

	FragColor = vec4(result, 1.0);
}
