#version 430 core

out vec4 FragColor;
in vec2  TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform vec2      screenSize;
uniform vec3      cameraPos;
uniform mat4      invView;
uniform mat4      invProjection;
uniform float     time;

uniform sampler3D u_brickPool;
uniform sampler2D blueNoiseTexture;

#include "../helpers/voxel_raymarch.glsl"

void main() {
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;
	float depth = texture(depthTexture, TexCoords).r;

	// Reconstruct scene world position to get depth limit
	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPosRel = invProjection * ndcPos;
	viewPosRel /= viewPosRel.w;
	vec4  worldPos = invView * viewPosRel;
	float sceneDistance = length(worldPos.xyz - cameraPos);

	// If background (sky), use a very large distance
	if (depth >= 0.999999)
		sceneDistance = 2000.0;

	// Ray direction from camera through pixel
	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

	// Sample jitter from blue noise with time-based animation
	float jitter = texture(blueNoiseTexture, TexCoords * (screenSize / 256.0) + vec2(time * 0.1)).r;

	// Raymarch through the sparse voxel volume with jitter and radius sampling
	vec4 res = raymarch_density_jittered(cameraPos, rayDir, sceneDistance, jitter, u_brickPool);

	float density = res.x;

	if (density > 0.0001) {
		// Use a hot orange/fire color for density visualization
		vec3 volumeColor = mix(vec3(1.0, 0.4, 0.05), vec3(1.0, 0.9, 0.7), clamp(density * 0.2, 0.0, 1.0));
		float alpha = clamp(density * 5.0, 0.0, 1.0);

		// Add some atmospheric/glow feel
		volumeColor += vec3(1.0, 0.5, 0.2) * alpha * 0.3;

		// Alpha blend over the scene color
		FragColor = vec4(mix(sceneColor, volumeColor, alpha), 1.0);
	} else {
		FragColor = vec4(sceneColor, 1.0);
	}
}
