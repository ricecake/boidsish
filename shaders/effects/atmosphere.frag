#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;

uniform vec3 cameraPos;
uniform mat4 invView;
uniform mat4 invProjection;

#include "../helpers/lighting.glsl"
#include "../helpers/noise.glsl"
#include "../helpers/fog.glsl"

float fbm(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		v += a * snoise(p);
		p *= 2.0;
		a *= 0.5;
	}
	return v;
}

// Simple Mie scattering approximation
float scatter(vec3 lightDir, vec3 viewDir, float g) {
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * 3.14159 * pow(1.0 + g2 - 2.0 * g * dot(lightDir, viewDir), 1.5));
}

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;

	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3 worldPos = (invView * viewSpacePosition).xyz;

	vec3  rayDir = normalize(worldPos - cameraPos);
	float dist = length(worldPos - cameraPos);

	if (depth == 1.0) {
		dist = 1000.0 * worldScale; // Assume sky is far
		worldPos = cameraPos + rayDir * dist;
	}

	// 1. Height Fog (Haze)
	float fogFactor = getHeightFog(cameraPos, worldPos, hazeDensity, 1.0 / (hazeHeight * worldScale + 0.001));
	vec3  currentHazeColor = hazeColor;

	// Add light scattering to fog
	vec3 scattering = vec3(0.0);
	for (int i = 0; i < num_lights; i++) {
		vec3  lightDir = normalize(lights[i].position - cameraPos);
		float s = scatter(lightDir, rayDir, 0.7);
		scattering += lights[i].color * s * lights[i].intensity * 0.05;
	}
	currentHazeColor += scattering;

	// 2. Cloud Layer
	float cloudFactor = 0.0;
	vec3  cloudFinalColor = vec3(0.0);

	float scaledCloudAltitude = cloudParams.y * worldScale;
	float scaledCloudThickness = cloudParams.z * worldScale;

	// Intersect with cloud layer (volume approximation)
	float t_start = (scaledCloudAltitude - cameraPos.y) / (rayDir.y + 0.000001);
	float t_end = (scaledCloudAltitude + scaledCloudThickness - cameraPos.y) / (rayDir.y + 0.000001);

	if (t_start > t_end) {
		float temp = t_start;
		t_start = t_end;
		t_end = temp;
	}

	t_start = max(t_start, 0.0);
	t_end = min(t_end, dist);

	if (t_start < t_end) {
		float cloudAcc = 0.0;
		int   samples = 6;
		float jitter = fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453);

		for (int i = 0; i < samples; i++) {
			float t = mix(t_start, t_end, (float(i) + jitter) / float(samples));
			vec3  p = cameraPos + rayDir * t;
			float h = (p.y - scaledCloudAltitude) / max(scaledCloudThickness, 0.001);
			float tapering = smoothstep(0.0, 0.2, h) * smoothstep(1.0, 0.5, h);

			float noise = fbm((p.xz / worldScale) * 0.015 + jitter * time * 0.0001 + (p.y / worldScale) * 0.02);
			// float d = smoothstep(0.2, 0.6, noise * (i + 1)) * cloudDensity;
			float d = smoothstep(0.2, 0.6, noise * (i + (1 - noise))) * cloudParams.x * tapering;

			cloudAcc += d;
		}
		cloudFactor = 1.0 - exp(-cloudAcc * (t_end - t_start) * 0.05 / float(samples));

		// Cloud lighting at the center of the cloud intersection
		vec3 intersect = cameraPos + rayDir * mix(t_start, t_end, 0.5);
		vec3 cloudScattering = vec3(0.0);
		for (int i = 0; i < num_lights; i++) {
			vec3  L = normalize(lights[i].position - intersect);
			float d = max(0.0, dot(vec3(0, 1, 0), L)); // Simple top-lighting
			float silver = pow(max(0.0, dot(rayDir, L)), 4.0) * 0.5;

			cloudScattering += lights[i].color * (d * 0.5 + 0.5 + silver) * lights[i].intensity;
		}

		cloudFinalColor = cloudColor * (ambient_light + cloudScattering * 0.5);

		// Apply fog to clouds based on their own distance
		float cloudFogFactor = getHeightFog(cameraPos, cameraPos + rayDir * t_start, hazeDensity, 1.0 / (hazeHeight * worldScale + 0.001));
		cloudFinalColor = mix(cloudFinalColor, currentHazeColor, cloudFogFactor);
	}

	// Combine everything
	vec3 foggedSceneColor = mix(sceneColor, currentHazeColor, fogFactor);
	vec3 result = mix(foggedSceneColor, cloudFinalColor, cloudFactor);

	FragColor = vec4(result, 1.0);
}
