#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;

uniform vec3 cameraPos;
uniform mat4 invView;
uniform mat4 invProjection;

uniform float hazeDensity;
uniform float hazeHeight;
uniform float cloudDensity;
uniform float cloudAltitude;
uniform float cloudThickness;

#include "../helpers/lighting.glsl"
#include "../helpers/noise.glsl"

vec3 worldPosFromDepth(float depth) {
	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec4 worldSpacePosition = invView * viewSpacePosition;
	return worldSpacePosition.xyz;
}

float getHeightFog(vec3 start, vec3 end, float density, float heightFalloff) {
	float dist = length(end - start);
	vec3  dir = (end - start) / dist;

	float fog;
	if (abs(dir.y) < 0.0001) {
		fog = density * exp(-heightFalloff * start.y) * dist;
	} else {
		fog = (density / (heightFalloff * dir.y)) * (exp(-heightFalloff * start.y) - exp(-heightFalloff * end.y));
	}
	return 1.0 - exp(-max(0.0, fog));
}

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

	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3  worldRay = (invView * vec4(normalize(viewSpacePosition.xyz), 0.0)).xyz;
	vec3  rayDir = normalize(worldRay);

	vec3 worldPos;
	float dist;
	if (depth == 1.0) {
		dist = 1000.0; // Assume sky is far
		worldPos = cameraPos + rayDir * dist;
	} else {
		worldPos = worldPosFromDepth(depth);
		dist = length(worldPos - cameraPos);
	}

	// 1. Height Fog (Haze)
	float fogFactor = getHeightFog(cameraPos, worldPos, hazeDensity, 1.0 / (hazeHeight + 0.001));
	vec3  fogColor = vec3(0.6, 0.7, 0.8); // Base haze color

	// Add light scattering to fog
	vec3 scattering = vec3(0.0);
	for (int i = 0; i < num_lights; i++) {
		vec3  lightDir = normalize(lights[i].position - cameraPos);
		float s = scatter(lightDir, rayDir, 0.7);
		scattering += lights[i].color * s * lights[i].intensity * 0.05;
	}
	fogColor += scattering;

	// 2. Cloud Layer
	float cloudFactor = 0.0;
	vec3  cloudColor = vec3(0.0);

	// Intersect with cloud layer (horizontal plane at cloudAltitude)
	float t_cloud = (cloudAltitude - cameraPos.y) / (rayDir.y + 0.00001);
	if (t_cloud > 0.0 && (t_cloud < dist || depth == 1.0)) {
		vec3  intersect = cameraPos + rayDir * t_cloud;
		float noise = fbm(intersect.xz * 0.02 + time * 0.02);
		float cloudAlpha = smoothstep(0.1, 0.6, noise) * cloudDensity;

		// Use cloudThickness to affect density based on look angle
		float pathLength = cloudThickness / max(0.01, abs(rayDir.y));
		cloudAlpha = 1.0 - exp(-cloudAlpha * pathLength * 0.05);

		// Thick clouds have multiple noise octaves and some volume approximation
		float noise2 = fbm(intersect.xz * 0.05 - time * 0.01);
		cloudAlpha *= (0.8 + 0.4 * noise2);

		vec3 baseCloudColor = vec3(0.95, 0.95, 1.0);

		// Cloud lighting
		vec3 cloudScattering = vec3(0.0);
		for (int i = 0; i < num_lights; i++) {
			vec3  L = normalize(lights[i].position - intersect);
			float d = max(0.0, dot(vec3(0, 1, 0), L)); // Simple top-lighting
			cloudScattering += lights[i].color * (d * 0.5 + 0.5) * lights[i].intensity;
		}

		cloudColor = baseCloudColor * (ambient_light + cloudScattering * 0.5);
		cloudFactor = cloudAlpha;
	}

	// Combine everything
	vec3 result = mix(sceneColor, cloudColor, cloudFactor);
	result = mix(result, fogColor, fogFactor);

	FragColor = vec4(result, 1.0);
}
