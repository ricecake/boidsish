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
uniform vec3  hazeColor;
uniform float cloudDensity;
uniform float cloudAltitude;
uniform float cloudThickness;
uniform vec3  cloudColorUniform;
// uniform float time;

#include "../helpers/lighting.glsl"
#include "../helpers/noise.glsl"

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

	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3  worldPos = (invView * viewSpacePosition).xyz;

	vec3  rayDir = normalize(worldPos - cameraPos);
	float dist = length(worldPos - cameraPos);

	if (depth == 1.0) {
		dist = 1000.0; // Assume sky is far
		worldPos = cameraPos + rayDir * dist;
	}

	// 1. Height Fog (Haze)
	float fogFactor = getHeightFog(cameraPos, worldPos, hazeDensity, 1.0 / (hazeHeight + 0.001));
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
	vec3  cloudColor = vec3(0.0);

	// Intersect with cloud layer (volume approximation)
	float t_start = (cloudAltitude - cameraPos.y) / (rayDir.y + 0.000001);
	float t_end = (cloudAltitude + cloudThickness - cameraPos.y) / (rayDir.y + 0.000001);

	if (t_start > t_end) {
		float temp = t_start;
		t_start = t_end;
		t_end = temp;
	}

	t_start = max(t_start, 0.0);
	t_end = min(t_end, dist);

	if (t_start < t_end) {
		float cloudAcc = 0.0;
		int   samples = 4;
		for (int i = 0; i < samples; i++) {
			float t = mix(t_start, t_end, (float(i) + 0.5) / float(samples));
			vec3  p = cameraPos + rayDir * t;
			float noise = fbm(p.xz * 0.015 + time * 0.015 + p.y * 0.02);
			float d = smoothstep(0.2, 0.6, noise) * cloudDensity;
			cloudAcc += d;
		}
		cloudFactor = 1.0 - exp(-cloudAcc * (t_end - t_start) * 0.05 / float(samples));

		// Cloud lighting at the center of the cloud intersection
		vec3  intersect = cameraPos + rayDir * mix(t_start, t_end, 0.5);
		vec3  cloudScattering = vec3(0.0);
		for (int i = 0; i < num_lights; i++) {
			vec3  L = normalize(lights[i].position - intersect);
			float d = max(0.0, dot(vec3(0, 1, 0), L)); // Simple top-lighting
			cloudScattering += lights[i].color * (d * 0.5 + 0.5) * lights[i].intensity;
		}

		cloudColor = cloudColorUniform * (ambient_light + cloudScattering * 0.5);
	}

	// Combine everything
	vec3 result = mix(sceneColor, cloudColor, cloudFactor);
	result = mix(result, currentHazeColor, fogFactor);

	FragColor = vec4(result, 1.0);
}
