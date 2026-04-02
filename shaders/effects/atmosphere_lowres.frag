#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthTexture;

uniform mat4 invView;
uniform mat4 invProjection;

uniform vec3 cloudColorUniform;

// Atmosphere common defines and includes
#include "../atmosphere/common.glsl"
#include "../helpers/clouds.glsl"
#include "../helpers/fast_noise.glsl"
#include "../helpers/lighting.glsl"
#include "helpers/math.glsl"

uniform sampler2D u_skyViewLUT;

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
	vec3  zenithRadiance = sampleSkyView(vec3(0, 1, 0)) * 4.0;

	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3 worldPos = (invView * viewSpacePosition).xyz;

	vec3  rayDir = normalize(worldPos - viewPos);
	float dist = length(worldPos - viewPos);

	float R_earth = kEarthRadius * 1000.0 * worldScale;
	if (depth == 1.0) {
		dist = 50000.0 * worldScale;
		worldPos = viewPos + rayDir * dist;
	}

	float baseFloor = (cloudAltitude - 10.0) * worldScale;
	float baseCeiling = (cloudAltitude + cloudThickness + 300.0) * worldScale;
	float R_floor = R_earth + baseFloor;
	float R_ceiling = R_earth + baseCeiling;

	vec3 earthCenter = vec3(viewPos.x, -R_earth, viewPos.z);
	vec3 relRo = viewPos - earthCenter;

	float t_start = 1e10;
	float t_end = -1e10;

	float t0, t1;
	if (intersectSphere(relRo, rayDir, R_ceiling, t0, t1)) {
		t_start = max(0.0, t0);
		t_end = t1;

		if (intersectSphere(relRo, rayDir, R_floor, t0, t1)) {
			if (t0 < 0.0) {
				t_start = max(t_start, t1);
			} else {
				t_end = min(t_end, t0);
			}
		}
	}

	t_end = min(t_end, dist);

	vec3  cloudColor = vec3(0.0);
	float cloudTransmittance = 1.0;

	if (t_start < t_end) {
		vec3 lightEnergy = vec3(0.0);

		int samples = 48;
		int shadow_samples = 4;

		float jitter = fastBlueNoise(TexCoords * 10.0 + vec2(sin(time * 0.07), cos(time * -0.05)));
		float stepSize = (t_end - t_start) / float(samples);

		vec3 local = viewPos + rayDir * t_start;
		float adjustedTime = dayTime * smoothstep(0, 1, dayTime) * smoothstep(24, 23, dayTime);
		vec3 localWeatherDelta = fastCurl3d((local.xyz / (1000 * worldScale)) + vec3(time*0.001));
		float localWeather = (fastWorley3d(vec3((local.xz + adjustedTime*localWeatherDelta.xz) / (4000 * worldScale), time * 0.005)) * 0.5 + 0.5);

		for (int i = 0; i < samples; i++) {
			float t = t_start + (float(i) + jitter) * stepSize;
			if (t > dist)
				break;

			vec3 p = viewPos + rayDir * t;
			vec3 p_curved = p;
			p_curved.y = length(p - earthCenter) - R_earth;

			float d = calculateCloudDensity(
				p_curved,
				localWeather,
				cloudAltitude,
				cloudThickness,
				cloudDensity,
				cloudCoverage,
				worldScale,
				time,
				false
			);
			if (d <= 0.01)
				continue;

			float stepDensity = d * stepSize * 0.005;
			float transmittanceAtStep = exp(-stepDensity);

			vec3 stepScattering = vec3(0.0);
			for (int j = 0; j < num_lights; j++) {
				if (lights[j].type != LIGHT_TYPE_DIRECTIONAL) {
					continue;
				}

				vec3  L = normalize(-lights[j].direction);
				float cosTheta = dot(rayDir, L);
				float phase = cloudPhase(cosTheta);

				float shadowDensity = 0.0;
				float shadowStepSize = (baseCeiling - baseFloor) / float(shadow_samples) * 0.1;
				for (int k = 0; k < shadow_samples; k++) {
					vec3 sp = p + L * (float(k) + 0.5) * shadowStepSize;
					vec3 sp_curved = sp;
					sp_curved.y = length(sp - earthCenter) - R_earth;

					shadowDensity += calculateCloudDensity(
						sp_curved,
						localWeather,
						cloudAltitude,
						cloudThickness,
						cloudDensity,
						cloudCoverage,
						worldScale,
						time,
						true
					);
				}
				float opticalDepthToLight = shadowDensity * shadowStepSize * 0.01;
				float shadowTerm = mix(beerPowder(opticalDepthToLight, d), exp(-opticalDepthToLight) * 0.5, 0.0);

				stepScattering += lights[j].color * shadowTerm * phase * lights[j].intensity * (j == 0 ? 10.0 : 2.0);
			}

			vec3 ambient = mix(ambient_light, zenithRadiance, 0.5) * 0.5;
			vec3 S = (stepScattering + ambient);

			lightEnergy += cloudTransmittance * S * stepDensity;
			cloudTransmittance *= transmittanceAtStep;

			if (cloudTransmittance < 0.01) {
				break;
			}
		}

		cloudColor = lightEnergy * cloudColorUniform;
	}

	FragColor = vec4(cloudColor, cloudTransmittance);
}
