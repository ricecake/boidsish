#version 430 core

out vec4 FragColor;
in vec2  TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform vec3      cameraPos;
uniform mat4      invView;
uniform mat4      invProjection;
uniform float     time;

#include "../helpers/particle_grid.glsl"

void main() {
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;
	float depth = texture(depthTexture, TexCoords).r;

	// Reconstruct ray direction in world space
	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

	// Determine max distance from depth buffer
	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPosRel = invProjection * ndcPos;
	viewPosRel /= viewPosRel.w;
	vec4  worldPos = invView * viewPosRel;
	float sceneDistance = length(worldPos.xyz - cameraPos);
	if (depth >= 0.999999)
		sceneDistance = 500.0;

	// Trace through the particle grid
	float maxDist = min(sceneDistance, 200.0);
	float stepSize = 1.0;
	float radius = 4.0;
	float density = trace_particle_density(cameraPos, rayDir, maxDist, stepSize, radius);

	// Simple volumetric coloring
	vec3 fogColor = vec3(0.5, 0.7, 1.0);
	float transmission = exp(-density * 0.05);

	vec3 finalColor = mix(fogColor, sceneColor, transmission);

	// Add some "glow" where density is high
	finalColor += fogColor * (1.0 - transmission) * 0.5;

	FragColor = vec4(finalColor, 1.0);
}
