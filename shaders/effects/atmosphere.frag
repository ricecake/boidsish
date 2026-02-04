#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;

uniform vec3 cameraPos;
uniform mat4 invView;
uniform mat4 invProjection;
uniform float time;

uniform float hazeDensity;
uniform float hazeHeight;
uniform vec3  hazeColor;
uniform float cloudDensity;
uniform float cloudAltitude;
uniform float cloudThickness;
uniform vec3  cloudColorUniform;

// New uniforms for enhanced clouds and atmosphere
uniform float hazeG = 0.7;
uniform float cloudG = 0.8;
uniform float cloudScatteringBoost = 2.0;
uniform float cloudPowderStrength = 0.5;

#include "../helpers/lighting.glsl"
#include "../helpers/noise.glsl"
#include "../helpers/atmosphere.glsl"
#include "lygia/generative/worley.glsl"

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

float cloudFBM(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	vec3  shift = vec3(time * 0.05, 0.0, time * 0.02);
	for (int i = 0; i < 3; i++) {
		v += a * worley(p + shift);
		p = p * 2.0;
		a *= 0.5;
	}
	return v;
}

float sampleCloudDensity(vec3 p) {
	float h = (p.y - cloudAltitude) / max(cloudThickness, 0.001);
	float tapering = smoothstep(0.0, 0.2, h) * smoothstep(1.0, 0.5, h);

	float noise = cloudFBM(p * 0.02);
	float d = smoothstep(0.3, 0.7, noise) * cloudDensity * tapering;
	return d;
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
		dist = 5000.0; // Assume sky is far
		worldPos = cameraPos + rayDir * dist;
	}

	// 1. Height Fog (Haze)
	float fogFactor = getHeightFog(cameraPos, worldPos, hazeDensity, 1.0 / (hazeHeight + 0.001));
	vec3  currentHazeColor = hazeColor;

	// Add light scattering to fog
	vec3 scattering = vec3(0.0);
	for (int i = 0; i < num_lights; i++) {
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			vec3  lightDir = normalize(-lights[i].direction);
			float s = henyeyGreensteinPhase(dot(lightDir, rayDir), hazeG);
			scattering += lights[i].color * s * lights[i].intensity * 0.05;
		}
	}
	currentHazeColor += scattering;

	// 2. Cloud Layer
	float cloudTransmittance = 1.0;
	vec3  cloudLight = vec3(0.0);

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
		float stepSize = (t_end - t_start) / float(samples);
		float jitter = fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453);

		vec3  sunDir = vec3(0, 1, 0);
		vec3  sunColor = vec3(1);
		for (int i = 0; i < num_lights; i++) {
			if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
				sunDir = normalize(-lights[i].direction);
				sunColor = lights[i].color * lights[i].intensity;
				break;
			}
		}

		float totalDensity = 0.0;
		for (int i = 0; i < samples; i++) {
			float t = t_start + (float(i) + jitter) * stepSize;
			vec3  p = cameraPos + rayDir * t;
			float d = sampleCloudDensity(p);

			if (d > 0.001) {
				// Self-shadowing towards sun
				float shadowDensity = 0.0;
				int   shadowSamples = 4;
				float shadowStep = cloudThickness * 0.2;
				for (int j = 1; j <= shadowSamples; j++) {
					vec3 sp = p + sunDir * shadowStep * float(j);
					shadowDensity += sampleCloudDensity(sp);
				}
				float transmittance = beerPowder(shadowDensity, shadowStep, cloudPowderStrength);

				// Scattering
				float phase = henyeyGreensteinPhase(dot(rayDir, sunDir), cloudG);
				vec3  pointLight = sunColor * transmittance * phase * cloudScatteringBoost;
				pointLight += ambient_light; // Ambient light in clouds

				vec3 src = pointLight * d * cloudColorUniform;
				cloudLight += src * cloudTransmittance * stepSize;
				cloudTransmittance *= exp(-d * stepSize);

				if (cloudTransmittance < 0.01)
					break;
			}
		}
	}

	// Combine everything
	vec3 result = sceneColor * cloudTransmittance + cloudLight;
	result = mix(result, currentHazeColor, fogFactor);

	FragColor = vec4(result, 1.0);
}
