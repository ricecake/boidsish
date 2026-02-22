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

// Simple Mie scattering approximation (Henyey-Greenstein phase function)
float miePhase(float cosTheta, float g) {
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

// Rayleigh scattering phase function
float rayleighPhase(float cosTheta) {
	return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
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

	// 1. Height Fog (Haze) with Night Adaptation
	float actualHazeDensity = hazeDensity * (1.0 - nightFactor * 0.7);
	vec3  actualHazeColor = mix(hazeColor, hazeColor * 0.05, nightFactor);

	float fogFactor = getHeightFog(cameraPos, worldPos, actualHazeDensity, 1.0 / (hazeHeight * worldScale + 0.001));
	vec3  currentHazeColor = actualHazeColor;

	// Add realistic light scattering (Mie + Rayleigh) to fog
	vec3  scattering = vec3(0.0);
	vec3  rayleighCoeff = vec3(5.8, 13.5, 33.1); // Relative coefficients for R, G, B wavelengths

	for (int i = 0; i < num_lights; i++) {
		vec3  L;
		float atten;
		calculateLightContribution(i, cameraPos, L, atten);

		float cosTheta = dot(L, rayDir);

		// Mie scattering (larger particles, forward-focused)
		float m = miePhase(cosTheta, 0.8);
		vec3  mieScattering = lights[i].color * m * lights[i].intensity * 0.04;

		// Rayleigh scattering (molecules, wavelength dependent blue bias)
		float r = rayleighPhase(cosTheta);
		vec3  rScattering = lights[i].color * r * lights[i].intensity * 0.015 * (rayleighCoeff * 0.05);

		scattering += (mieScattering + rScattering) * atten;
	}
	currentHazeColor += scattering;

	// 2. Cloud Layer
	float cloudFactor = 0.0;
	vec3  cloudColor = vec3(0.0);

	float scaledCloudAltitude = cloudAltitude * worldScale;
	float scaledCloudThickness = cloudThickness * worldScale;

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
			float d = smoothstep(0.2, 0.6, noise * (i + (1 - noise))) * cloudDensity * tapering;

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

		cloudColor = cloudColorUniform * (ambient_light + cloudScattering * 0.5);
	}

	// Combine everything
	vec3 result = mix(sceneColor, cloudColor, cloudFactor);
	result = mix(result, currentHazeColor, fogFactor);

	FragColor = vec4(result, 1.0);
}
