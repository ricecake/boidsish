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
	vec3 jitter = vec3(
		random(TexCoords + time) - 0.5,
		random(TexCoords + time + 1.234) - 0.5,
		random(TexCoords + time + 5.678) - 0.5
	);
	vec3 normalWS = normalize(worldNormal + jitter * roughness * 0.2);

	vec3 reflectDirWS = reflect(viewDirWS, normalWS);

	// Raymarching in world space
	vec3 currentPosWS = worldPos;
	// Dynamic step size based on distance? Let's start with fixed.
	vec3  rayStepWS = reflectDirWS * 0.5;
	vec4  hitColor = vec4(0.0);
	float hitOccurred = 0.0;

	// Optimization: skip very small reflections
	if (dot(worldNormal, reflectDirWS) < 0.0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

	for (int i = 0; i < 32; i++) {
		currentPosWS += rayStepWS;

		vec4 projectedPos = projection * view * vec4(currentPosWS, 1.0);
		if (projectedPos.w <= 0.0)
			break;
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
		if (linearRayDepth < linearSampleDepth) {
			// Check if it's a hit (thickness check)
			if (abs(linearRayDepth - linearSampleDepth) < 1.0) {
				hitColor = texture(sceneTexture, sampleUV);
				hitOccurred = 1.0;
				break;
			}
		}
		// Increase step size
		rayStepWS *= 1.1;
	}

	vec4  sceneColor = texture(sceneTexture, TexCoords);
	float reflectionStrength = 0.7 * (1.0 - roughness);

	// Fresnel
	float fresnel = pow(1.0 - max(dot(worldNormal, -viewDirWS), 0.0), 5.0);
	reflectionStrength = mix(reflectionStrength, 1.0, fresnel);

	// Edge fade
	vec2  dUV = abs(TexCoords - 0.5) * 2.0;
	float edgeFade = 1.0 - max(pow(dUV.x, 8.0), pow(dUV.y, 8.0));

	FragColor = mix(sceneColor, hitColor, reflectionStrength * hitOccurred * edgeFade);
}
