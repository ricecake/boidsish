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

#define PI 3.14159265359

uniform float hazeG;
uniform float cloudG;

// Henyey-Greenstein phase function
float phaseHG(float g, float cosTheta) {
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
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
		dist = 1000.0; // Assume sky is far
		worldPos = cameraPos + rayDir * dist;
	}

	// 1. Height Fog (Haze)
	float fogFactor = getHeightFog(cameraPos, worldPos, hazeDensity, 1.0 / (hazeHeight + 0.001));

	// Add gentle texture to fog using 3D noise
	float fogTexture = fbm(worldPos * 0.02 + time * 0.01);
	fogFactor *= (0.7 + 0.6 * fogTexture);

	vec3 currentHazeColor = hazeColor;

	// Add light scattering to fog using Henyey-Greenstein phase function
	vec3 scattering = vec3(0.0);
	for (int i = 0; i < num_lights; i++) {
		vec3  lightDir = normalize(lights[i].position - cameraPos);
		float cosTheta = dot(lightDir, rayDir);
		float s = phaseHG(hazeG, cosTheta);
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
		int   samples = 32;
		float jitter = fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453);
		float stepSize = (t_end - t_start) / float(samples);

		vec3  cloudTotalScattering = vec3(0.0);
		float totalAlpha = 0.0;

		for (int i = 0; i < samples; i++) {
			float t = t_start + (float(i) + jitter) * stepSize;
			vec3  p = cameraPos + rayDir * t;
			float h = (p.y - cloudAltitude) / max(cloudThickness, 0.001);
			float tapering = smoothstep(0.0, 0.2, h) * smoothstep(1.0, 0.5, h);

			// Improved 3D noise for organic shape
			float noise = fbm(p * 0.015 + time * 0.01);
			float density = smoothstep(0.3, 0.7, noise) * cloudDensity * tapering;

			if (density > 0.01) {
				// Self-shadowing: raymarch towards the first light source (assumed primary)
				float shadowAcc = 0.0;
				if (num_lights > 0) {
					int   shadowSamples = 4;
					vec3  L = normalize(lights[0].position - p);
					float shadowStepSize = 1.5;
					for (int j = 1; j <= shadowSamples; j++) {
						vec3  sp = p + L * float(j) * shadowStepSize;
						float sh = (sp.y - cloudAltitude) / max(cloudThickness, 0.001);
						float stapering = smoothstep(0.0, 0.2, sh) * smoothstep(1.0, 0.5, sh);
						float snoise = fbm(sp * 0.015 + time * 0.01);
						shadowAcc += smoothstep(0.3, 0.7, snoise) * cloudDensity * stapering;
					}
				}
				float shadowTransmittance = exp(-shadowAcc * 0.5);

				// Light scattering
				vec3 lightScattering = vec3(0.0);
				for (int li = 0; li < num_lights; li++) {
					vec3  L_li = normalize(lights[li].position - p);
					float cosTheta = dot(L_li, rayDir);
					float s = phaseHG(cloudG, cosTheta);

					// Rim lighting approximation for that "silver lining" effect
					float rim = pow(max(0.0, dot(rayDir, L_li)), 8.0) * 0.5;

					lightScattering += lights[li].color * (s * 0.5 + rim) * lights[li].intensity;
				}

				vec3 stepColor = cloudColorUniform * (ambient_light + lightScattering * shadowTransmittance);

				// Absorption and scattering (Beer's Law)
				float alpha = 1.0 - exp(-density * stepSize * 1.5);
				cloudTotalScattering += stepColor * alpha * (1.0 - totalAlpha);
				totalAlpha += alpha * (1.0 - totalAlpha);

				if (totalAlpha > 0.99)
					break;
			}
		}
		cloudFactor = totalAlpha;
		cloudColor = cloudTotalScattering;
	}

	// Combine everything
	// Cloud blending: cloudColor is pre-multiplied by alpha (totalAlpha)
	vec3 result = sceneColor * (1.0 - cloudFactor) + cloudColor;

	// Fog blending: standard exponential fog blend
	fogFactor = clamp(fogFactor, 0.0, 1.0);
	result = mix(result, currentHazeColor, fogFactor);

	FragColor = vec4(result, 1.0);
}
