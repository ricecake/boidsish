#version 430 core

layout(location = 0) out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;

#include "../temporal_data.glsl"
#include "../lighting.glsl"

vec3 worldPosFromDepth(float d) {
	vec4 clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
	vec4 worldSpacePosition = invView * invProjection * clipSpacePosition;
	return worldSpacePosition.xyz / worldSpacePosition.w;
}

void main() {
	float d = texture(depthTexture, TexCoords).r;
	if (d >= 1.0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

	vec3 worldPos = worldPosFromDepth(d);

	vec3 lightDir;
	if (lights[0].type == 1) { // DIRECTIONAL
		lightDir = normalize(-lights[0].direction);
	} else {
		lightDir = normalize(lights[0].position - worldPos);
	}

	// Simple Screen Space Shadow Raymarch
	float shadow = 1.0;
	float stepSize = 0.5;
	int numSteps = 16;
	vec3 rayPos = worldPos + lightDir * stepSize;

	for (int i = 0; i < numSteps; ++i) {
		vec4 proj = viewProjection * vec4(rayPos, 1.0);
		vec3 screenPos = proj.xyz / proj.w;
		vec2 uv = screenPos.xy * 0.5 + 0.5;

		if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;

		float sampledDepth = texture(depthTexture, uv).r;
		float currentDepth = screenPos.z * 0.5 + 0.5;

		if (currentDepth > sampledDepth + 0.0001) {
			shadow = 0.5; // Soften the SSS shadow
			break;
		}

		rayPos += lightDir * stepSize;
	}

	vec3 color = texture(sceneTexture, TexCoords).rgb;
	FragColor = vec4(color * shadow, 1.0);
}
