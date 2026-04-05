#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;

uniform mat4 invView;
uniform mat4 invProjection;

uniform float hazeDensity;
uniform float hazeHeight;
uniform vec3  hazeColor;
uniform vec3  cloudColorUniform;

// u_transmittanceLUT is declared in helpers/lighting.glsl
uniform sampler2D u_skyViewLUT;
uniform sampler3D u_aerialPerspectiveLUT;

#include "../helpers/lighting.glsl"
#include "../atmosphere/common.glsl"
#include "../helpers/clouds.glsl"
#include "../helpers/fast_noise.glsl"
#include "helpers/math.glsl"

vec3 sampleAerialPerspective(vec3 rd, float distKM) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float v = elevation / PI + 0.5;
	float w = distKM / 32.0; // maxDist in AP LUT

	return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).rgb;
}

float sampleAerialPerspectiveTransmittance(vec3 rd, float distKM) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float v = elevation / PI + 0.5;
	float w = distKM / 32.0;

	return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).a;
}

vec3 sampleSkyView(vec3 rd) {
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;
	float elevation = asin(clamp(rd.y, -1.0, 1.0));

	float u = azimuth / (2.0 * PI);
	float adjV = sqrt(abs(elevation) / (PI * 0.5));
	float v = (elevation < 0.0) ? (1.0 - adjV) * 0.5 : (adjV + 1.0) * 0.5;

	return texture(u_skyViewLUT, vec2(u, v)).rgb;
}

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;
	vec3  zenithRadiance = sampleSkyView(vec3(0, 1, 0)) * 4.0;

	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3 worldPos = (invView * viewSpacePosition).xyz;

	vec3  rayDir = normalize(worldPos - viewPos);
	float dist = length(worldPos - viewPos);

	if (depth == 1.0) {
		dist = 50000.0 * worldScale;
		worldPos = viewPos + rayDir * dist;
	}

	// Use hazeDensity to scale the effective distance in the atmosphere
	float distKM = (dist / 1000.0) * (hazeDensity * 300.0);
	vec3  inScattering = sampleAerialPerspective(rayDir, distKM);
	float transmittance = sampleAerialPerspectiveTransmittance(rayDir, distKM);

	// Cloud Layer
	vec3  cloudColor = vec3(0.0);
	float cloudTransmittance = 1.0;

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	// Wide vertical bounds for intersection (covering all possible dynamic offsets)
	float baseFloor = (cloudAltitude - 100.0) * worldScale;
	float baseCeiling = (cloudAltitude + cloudThickness + 1100.0) * worldScale;

	float t_start = (baseFloor - viewPos.y) / (rayDir.y + 0.000001);
	float t_end = (baseCeiling - viewPos.y) / (rayDir.y + 0.000001);

	if (t_start > t_end) {
		float temp = t_start;
		t_start = t_end;
		t_end = temp;
	}
	t_start = max(t_start, 0.0);
	t_end = min(t_end, dist);

	if (t_start < t_end) {
		vec3  lightEnergy = vec3(0.0);

		int samples = 48;
		int shadow_samples = 4;

		float jitter = fastBlueNoise(TexCoords * 10.0 + vec2(sin(time * 0.07), cos(time * -0.05)));
		float stepSize = (t_end - t_start) / float(samples);

		for (int i = 0; i < samples; i++) {
			float t = t_start + (float(i) + jitter) * stepSize;
			if (t > dist)
				break;

			vec3 p = viewPos + rayDir * t;

			// Sample weather at current ray position to avoid depth dependency
			float weatherWarpFactor = 1.0;
			if (cloudWarp > 0.0) {
				float camDist = length(p.xz - viewPos.xz);
				weatherWarpFactor = smoothstep(0.0, cloudWarp * worldScale, camDist);
			}

			vec2 weatherUV = p.xz / (4000.0 * worldScale);
			float weatherMap = weatherWarpFactor * (fastWorley3d(vec3(weatherUV, time * 0.01)) * 0.5 + 0.5);

			vec2 heightUV = p.xz / (2500.0 * worldScale);
			float heightMap = weatherWarpFactor * (fastWorley3d(vec3(heightUV, time * 0.004)) * 0.5 + 0.5);

			CloudWeather weather;
			weather.weatherMap = weatherMap;
			weather.heightMap = heightMap;

			CloudLayer layer = computeCloudLayer(weather, props);

			float d = calculateCloudDensity(
				p,
				weather,
				layer,
				props,
				time,
				false
			);
			if (d <= 0.01)
				continue;

			float stepDensity = d * stepSize * 0.005;
			float transmittanceAtStep = exp(-stepDensity);

			vec3 stepScattering = vec3(0.0);
			for (int j = 0; j < num_lights; j++) {
				if (lights[j].type != LIGHT_TYPE_DIRECTIONAL)
					continue;

				vec3  L = normalize(-lights[j].direction);
				float cosTheta = dot(rayDir, L);
				float phase = cloudPhase(cosTheta);

				float shadowDensity = 0.0;
				float shadowStepSize = layer.thickness / float(shadow_samples) * cloudShadowStepMultiplier;
				for (int k = 0; k < shadow_samples; k++) {
					vec3 sp = p + L * (float(k) + 0.5) * shadowStepSize;
					shadowDensity += calculateCloudDensity(
						sp,
						weather,
						layer,
						props,
						time,
						true
					);
				}
				float opticalDepthToLight = shadowDensity * shadowStepSize * cloudShadowOpticalDepthMultiplier;
				float shadowTerm = mix(beerPowder(opticalDepthToLight, d), exp(-opticalDepthToLight), cloudBeerPowderMix);

				stepScattering += lights[j].color * shadowTerm * phase * lights[j].intensity * (j == 0 ? cloudSunLightScale : cloudMoonLightScale);
			}

			vec3 ambient = mix(ambient_light, zenithRadiance, 0.5) * 0.5;
			vec3 S = (stepScattering + ambient);

			lightEnergy += cloudTransmittance * S * stepDensity;
			cloudTransmittance *= transmittanceAtStep;

			if (cloudTransmittance < 0.01)
				break;
		}

		cloudColor = lightEnergy * cloudColorUniform;
	}

	// Combine everything
	vec3 result = sceneColor * cloudTransmittance + cloudColor;
	// Apply physically accurate atmosphere
	if (depth < 1.0) {
		result = result * transmittance + inScattering;
	}

	FragColor = vec4(result, 1.0);
}
