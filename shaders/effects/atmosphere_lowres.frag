#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthTexture;
// u_transmittanceLUT is declared in helpers/lighting.glsl

uniform mat4 invView;
uniform mat4 invProjection;

uniform vec3 cloudColorUniform;

// Atmosphere common defines and includes
#include "../helpers/lighting.glsl"
#include "../atmosphere/common.glsl"
#include "../helpers/clouds.glsl"
#include "../helpers/fast_noise.glsl"
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


vec3 evalSHIrradiance(vec3 n) {
	// Constants for SH basis functions
	float c1 = 0.282095;
	float c2 = 0.488603;
	float c3 = 1.092548;
	float c4 = 0.315392;
	float c5 = 0.546274;

	// Cosine lobe convolution (irradiance) coefficients
	// A0 = PI, A1 = 2*PI/3, A2 = PI/4
	float a0 = 3.141593;
	float a1 = 2.094395;
	float a2 = 0.785398;

	vec3 res = vec3(0.0);

	// L0
	res += a0 * c1 * sh_coeffs[0].rgb;

	// L1
	res += a1 * c2 * (sh_coeffs[1].rgb * n.y + sh_coeffs[2].rgb * n.z + sh_coeffs[3].rgb * n.x);

	// L2
	res += a2 * c3 * (sh_coeffs[4].rgb * n.x * n.y + sh_coeffs[5].rgb * n.y * n.z + sh_coeffs[7].rgb * n.x * n.z);
	res += a2 * c4 * sh_coeffs[6].rgb * (3.0 * n.z * n.z - 1.0);
	res += a2 * c5 * sh_coeffs[8].rgb * (n.x * n.x - n.y * n.y);

	return max(res, 0.0);
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

	// Dynamic cloud parameters
	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	// Wide vertical bounds for intersection (covering all possible dynamic offsets)
	float R_floor = R_earth + (cloudAltitude - 100.0) * worldScale;
	float R_ceiling = R_earth + (cloudAltitude + cloudThickness + 1100.0) * worldScale;

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

		for (int i = 0; i < samples; i++) {
			float t = t_start + (float(i) + jitter) * stepSize;
			if (t > dist)
				break;

			vec3 p = viewPos + rayDir * t;
			vec3 p_curved = p;
			p_curved.y = length(p - earthCenter) - R_earth;

			// Sample weather at current ray position to avoid depth dependency
			float weatherWarpFactor = 1.0;
			vec3 p_curved_warped = vec3(0);
			if (cloudWarp > 0.0) {
				float camDist = length(p.xz - viewPos.xz);
				// weatherWarpFactor = smoothstep(0.0, cloudWarp * worldScale, camDist);
				p_curved_warped = getWarpedCloudPos(p_curved, weatherWarpFactor);
			}

			vec2 weatherUV = p.xz / (4000.0 * worldScale);
			float weatherMap = weatherWarpFactor * (fastWorley3d(vec3(weatherUV, time * 0.001)) * 0.5 + 0.5);

			vec2 heightUV = p.xz / (2500.0 * worldScale);
			float heightMap = weatherWarpFactor * (fastWorley3d(vec3(heightUV, time * 0.0004)) * 0.5 + 0.5);

			CloudWeather weather;
			weather.weatherMap = weatherMap;
			weather.heightMap = heightMap;

			CloudLayer layer = computeCloudLayer(weather, props);

			float d = calculateCloudDensity(
				p_curved_warped,
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
				if (lights[j].type != LIGHT_TYPE_DIRECTIONAL) {
					continue;
				}

				vec3  L = normalize(-lights[j].direction);

				vec3 voxelToCenter = p - earthCenter;
				float r = length(voxelToCenter);
				float r_world = length(p - earthCenter);
				float r_km = r_world / (1000.0 * worldScale);
				float mu = dot(voxelToCenter / r, L);
				float cosTheta = dot(rayDir, L);
				float phase = cloudPhase(cosTheta);

				float shadowDensity = 0.0;
				// float shadowStepSize = layer.thickness / float(shadow_samples) * cloudShadowStepMultiplier;
				// intersectSphere(p, L, R_ceiling);

				float st_start = 1e10;
				float st_end = -1e10;

				float t0, t1;
				if (intersectSphere(p, L, R_ceiling, t0, t1)) {
					st_start = max(0.0, t0);
					st_end = t1;

					if (intersectSphere(p, L, R_floor, t0, t1)) {
						if (t0 < 0.0) {
							st_start = max(t_start, t1);
						} else {
							st_end = min(t_end, t0);
						}
					}
				}

				// vec2 transUV = transmittanceToUV(r, mu);
				vec2 transUV = transmittanceToUV(r_km, mu);
				vec3 sunTransmittance = texture(u_transmittanceLUT, transUV).rgb;

				float shadowStepSize = (st_end - st_start) / float(shadow_samples);
				for (int k = 0; k < shadow_samples; k++) {
					vec3 sp = p + L * (float(k) + 0.5) * shadowStepSize;
					vec3 sp_curved = sp;
					sp_curved.y = length(sp - earthCenter) - R_earth;

					shadowDensity += calculateCloudDensity(
						sp_curved,
						weather,
						layer,
						props,
						time,
						true
					);
				}
				float opticalDepthToLight = shadowDensity * shadowStepSize * cloudShadowOpticalDepthMultiplier;
				float shadowTerm = mix(beerPowder(opticalDepthToLight, d), exp(-opticalDepthToLight), cloudBeerPowderMix);

				// stepScattering += sunTransmittance * lights[j].color * shadowTerm * phase * lights[j].intensity * (j == 0 ? cloudSunLightScale : cloudMoonLightScale);
				stepScattering += sunTransmittance * shadowTerm * phase * lights[j].intensity * (j == 0 ? cloudSunLightScale : cloudMoonLightScale);
			}

			// Use SH-based irradiance for high-fidelity ambient lighting in clouds
			// Using the up vector (0,1,0) as a simple approximation for the ambient sky light
			// that hits the cloud volume.
			vec3 ambient = evalSHIrradiance(vec3(0, 1, 0));
			vec3 S = (stepScattering + ambient);

			// lightEnergy += cloudTransmittance * S * stepDensity;
			lightEnergy += cloudTransmittance * S * (1.0 - transmittanceAtStep);
			cloudTransmittance *= transmittanceAtStep;

			if (cloudTransmittance < 0.01) {
				break;
			}
		}

		cloudColor = lightEnergy;// * cloudColorUniform;
	}

	FragColor = vec4(cloudColor, cloudTransmittance);
}
