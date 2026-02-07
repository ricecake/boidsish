#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D transmittanceLUT;
uniform sampler3D cloudNoiseLUT;
uniform sampler3D cloudDetailNoiseLUT;
uniform sampler2D curlNoiseLUT;
uniform sampler2D weatherMap;

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

uniform float hazeG = 0.7;
uniform float cloudG = 0.8;
uniform float cloudScatteringBoost = 2.0;
uniform float cloudPowderStrength = 0.5;

// HZD specific parameters
uniform float cloudCoverage = 0.5;
uniform float cloudType = 0.5;
uniform float cloudWindSpeed = 5.0;
uniform vec3  cloudWindDir = vec3(1.0, 0.0, 0.5);
uniform float cloudDetailScale = 1.0;
uniform float cloudCurlStrength = 0.5;

#include "../helpers/atmosphere.glsl"
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

float remap(float value, float original_min, float original_max, float new_min, float new_max) {
	return new_min + (((value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

// Vertical gradient based on cloud type
// type: 0.0 = Stratus, 0.5 = Stratocumulus, 1.0 = Cumulus
float getCloudGradient(float heightFraction, float type) {
	float stratus = smoothstep(0.0, 0.1, heightFraction) * (1.0 - smoothstep(0.2, 0.3, heightFraction));
	float stratocumulus = smoothstep(0.0, 0.2, heightFraction) * (1.0 - smoothstep(0.7, 0.9, heightFraction));
	float cumulus = smoothstep(0.0, 0.1, heightFraction) * (1.0 - smoothstep(0.8, 1.0, heightFraction));

	if (type < 0.5) {
		return mix(stratus, stratocumulus, type * 2.0);
	} else {
		return mix(stratocumulus, cumulus, (type - 0.5) * 2.0);
	}
}

float sampleCloudDensity(vec3 p, bool fullDetail) {
	float heightFraction = (p.y - cloudAltitude) / cloudThickness;
	if (heightFraction < 0.0 || heightFraction > 1.0)
		return 0.0;

	vec3 windOffset = cloudWindDir * time * cloudWindSpeed;
	vec3 p_wind = p + windOffset;

	// 1. Weather Map
	vec2 weatherUV = p_wind.xz * 0.0001;
	vec4 weather = texture(weatherMap, weatherUV);
	float coverage = mix(0.1, 0.9, weather.r) * cloudCoverage;
	float type = weather.g * cloudType;

	// 2. Base Noise
	vec3 p_base = p_wind * 0.0005;
	vec4 baseNoise = texture(cloudNoiseLUT, p_base);
	float baseDensity = baseNoise.r; // Perlin-Worley

	// Apply tiered Worley for more shape
	float lowFreqWorley = baseNoise.g * 0.625 + baseNoise.b * 0.25 + baseNoise.a * 0.125;
	baseDensity = remap(baseDensity, lowFreqWorley * 0.2, 1.0, 0.0, 1.0);

	// 3. Vertical Gradient
	baseDensity *= getCloudGradient(heightFraction, type);

	// 4. Coverage
	float density = remap(baseDensity, 1.0 - coverage, 1.0, 0.0, 1.0);
	density *= coverage;

	if (density > 0.0 && fullDetail) {
		// 5. Curl Noise & Detail Erosion
		vec2 curl = texture(curlNoiseLUT, p_wind.xz * 0.001).rg * 2.0 - 1.0;
		vec3 p_detail = (p_wind + vec3(curl * 100.0 * heightFraction * cloudCurlStrength, 0.0)) * 0.005 * cloudDetailScale;

		vec4 detailNoise = texture(cloudDetailNoiseLUT, p_detail);
		float detailFBM = detailNoise.r * 0.625 + detailNoise.g * 0.25 + detailNoise.b * 0.125;

		float erosion = mix(detailFBM, 1.0 - detailFBM, clamp(heightFraction * 3.0, 0.0, 1.0));
		density = remap(density, erosion * 0.2, 1.0, 0.0, 1.0);
	}

	return clamp(density * cloudDensity, 0.0, 1.0);
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
		dist = 100000.0;
		worldPos = cameraPos + rayDir * dist;
	}

	// 1. Height Fog (Haze)
	float fogFactor = getHeightFog(cameraPos, worldPos, hazeDensity, 1.0 / (hazeHeight + 0.001));

	// Add scattering to haze
	vec3 sunDir = vec3(0, 1, 0);
	vec3 sunColor = vec3(1);
	for (int i = 0; i < num_lights; i++) {
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			sunDir = normalize(-lights[i].direction);
			sunColor = lights[i].color * lights[i].intensity;
			break;
		}
	}

	vec3 currentHazeColor = calculateSkyColor(transmittanceLUT, rayDir, sunDir, sunColor);

	// 2. Cloud Layer
	float cloudTransmittance = 1.0;
	vec3  cloudLight = vec3(0.0);

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
		int   samples = 64;
		float stepSize = (t_end - t_start) / float(samples);
		float jitter = fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453);

		for (int i = 0; i < samples; i++) {
			float t = t_start + (float(i) + jitter) * stepSize;
			vec3  p = cameraPos + rayDir * t;

			// Sample density (full detail only if not too deep in cloud)
			float d = sampleCloudDensity(p, cloudTransmittance > 0.1);

			if (d > 0.001) {
				// Self-shadowing (HZD style simple: sample density towards sun)
				float shadowDensity = sampleCloudDensity(p + sunDir * 3.0, false) * 2.0;
				shadowDensity += sampleCloudDensity(p + sunDir * 8.0, false) * 1.0;

				float T_internal = exp(-shadowDensity * 0.3);
				vec3 T_sun = getTransmittance(transmittanceLUT, p.y, sunDir.y);

				float powder = beerPowder(d, stepSize, cloudPowderStrength);
				vec3  effectiveTransmittance = T_sun * T_internal * powder;

				float phase = henyeyGreensteinPhase(dot(rayDir, sunDir), cloudG);
				vec3  pointLight = sunColor * effectiveTransmittance * phase * cloudScatteringBoost;
				pointLight += ambient_light * 0.1;

				vec3 src = pointLight * d * cloudColorUniform;
				cloudLight += src * cloudTransmittance * stepSize;
				cloudTransmittance *= exp(-d * stepSize);

				if (cloudTransmittance < 0.01)
					break;
			}
		}
	}

	// Combine everything
	// If depth is 1.0, sceneColor IS the sky.
	// Clouds should attenuate the sky and add their light.
	vec3 result = sceneColor * cloudTransmittance + cloudLight;

	// Apply haze (fog) to the result.
	// But only if it's closer than the "far plane" distance for the sky.
	float effectiveFogFactor = fogFactor;
	if (depth == 1.0) {
		// For sky, the fog is already "baked" into calculateSkyColor in sky.frag
		// and currentHazeColor here.
		// We don't want to double-fog the clouds if they are part of the sky.
		// However, haze (height fog) should still affect them.
		effectiveFogFactor = fogFactor * 0.5; // Reduce fog impact on sky-depth clouds
	}

	result = mix(result, currentHazeColor, effectiveFogFactor);

	FragColor = vec4(result, 1.0);
}
