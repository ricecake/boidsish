#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D normalTexture;
uniform sampler2D pbrTexture;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 invProjection;
uniform mat4 invView;
uniform float time;
uniform vec3 viewPos;

#include "lygia/generative/random.glsl"

// Better hash for jitter
float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 getPos(vec2 uv) {
	float depth = texture(depthTexture, uv).r;
	vec4  clipSpacePosition = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	return viewSpacePosition.xyz / viewSpacePosition.w;
}

vec3 getNormal(vec2 uv) {
	return texture(normalTexture, uv).xyz;
}

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	if (depth >= 1.0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

	vec3 posVS = getPos(TexCoords);
	vec3 worldPos = (invView * vec4(posVS, 1.0)).xyz;
	vec3 worldNormal = normalize(getNormal(TexCoords));
	vec2 pbr = texture(pbrTexture, TexCoords).rg;
	float roughness = pbr.r;

	vec3 viewDirWS = normalize(worldPos - viewPos);

	// Stochastic part: jitter the normal based on roughness
	// Use a slightly more stable jitter
	float h = hash(TexCoords + time);
	vec3 jitter = vec3(
		hash(TexCoords + vec2(h, 0.0)) - 0.5,
		hash(TexCoords + vec2(0.0, h)) - 0.5,
		hash(TexCoords + vec2(h, h)) - 0.5
	);
	vec3 normalWS = normalize(worldNormal + jitter * roughness * 0.4);

	vec3 reflectDirWS = reflect(viewDirWS, normalWS);

	// Raymarching in world space
	// Add a small bias to the start position to avoid self-intersection
	vec3 currentPosWS = worldPos + worldNormal * 0.05;
	// Initial step size scaled by distance to handle larger scales better
	float distToCam = length(worldPos - viewPos);
	vec3  rayStepWS = reflectDirWS * mix(0.1, 0.5, clamp(distToCam / 100.0, 0.0, 1.0));
	vec4  hitColor = vec4(0.0);
	float hitOccurred = 0.0;

	// Optimization: skip very small reflections
	if (dot(worldNormal, reflectDirWS) < 0.0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

	// Better thickness check based on distance
	float thickness = 0.5 * mix(1.0, 10.0, clamp(distToCam / 500.0, 0.0, 1.0));

	for (int i = 0; i < 40; i++) {
		currentPosWS += rayStepWS;

		vec4 projectedPos = projection * view * vec4(currentPosWS, 1.0);
		if (projectedPos.w <= 0.0) {
			// Ray went behind camera, try to sample horizon/sky
			break;
		}
		projectedPos.xyz /= projectedPos.w;
		vec2 sampleUV = projectedPos.xy * 0.5 + 0.5;

		if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
			break;

		float sampleDepth = texture(depthTexture, sampleUV).r;
		if (sampleDepth >= 1.0)
			continue;

		// Convert sample depth to linear view space depth for comparison
		vec4  sampleClipPos = vec4(sampleUV * 2.0 - 1.0, sampleDepth * 2.0 - 1.0, 1.0);
		vec4  sampleViewPos = invProjection * sampleClipPos;
		float linearSampleDepth = sampleViewPos.z / sampleViewPos.w;

		vec4  currentViewPos = view * vec4(currentPosWS, 1.0);
		float linearRayDepth = currentViewPos.z / currentViewPos.w;

		// Check if ray is behind geometry
		// Since linear depth is negative in OpenGL, linearRayDepth < linearSampleDepth means further away
		if (linearRayDepth < linearSampleDepth) {
			// Check if it's a hit (thickness check)
			if (abs(linearRayDepth - linearSampleDepth) < thickness) {
				hitColor = texture(sceneTexture, sampleUV);
				hitOccurred = 1.0;
				break;
			}
		}
		// Increase step size
		rayStepWS *= 1.1;
	}

	vec4  sceneColor = texture(sceneTexture, TexCoords);
	// Boost reflection for smooth surfaces
	float reflectionStrength = mix(0.8, 0.1, roughness);

	// Fresnel - stronger at grazing angles
	float fresnel = pow(1.0 - max(dot(worldNormal, -viewDirWS), 0.0), 5.0);
	reflectionStrength = mix(reflectionStrength, 1.0, fresnel);

	// Edge fade
	vec2  dUV = abs(TexCoords - 0.5) * 2.0;
	float edgeFade = 1.0 - clamp(max(pow(dUV.x, 8.0), pow(dUV.y, 8.0)), 0.0, 1.0);

	// Fade out based on distance to prevent sharp cuts at far plane
	float distFade = 1.0 - smoothstep(400.0, 600.0, distToCam);

	FragColor = mix(sceneColor, hitColor, reflectionStrength * hitOccurred * edgeFade * distFade);
}
