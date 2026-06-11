#version 460 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out float CloudDepth;

in vec2 TexCoords;

uniform sampler2D depthTexture;
// u_transmittanceLUT is declared in helpers/lighting.glsl

uniform mat4 invView;
uniform mat4 invProjection;

uniform vec3 cloudColorUniform;

// Atmosphere common defines and includes
#include "../atmosphere/common.glsl"
#include "../helpers/clouds.glsl"
#include "../helpers/fast_noise.glsl"
#include "../helpers/lighting.glsl"
#include "helpers/math.glsl"
#include "lygia/generative/wavelet.glsl"
#include "lygia/generative/voronoi.glsl"
#include "lygia/generative/random.glsl"

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

// Gets the spherical position relative to the planet center
// This keeps sampling consistent along the shell regardless of viewing angle
vec3 getSphericalCoords(vec3 p, vec3 earthCenter, float R_earth) {
    vec3 dir = normalize(p - earthCenter);
    float altitude = length(p - earthCenter) - R_earth;

    // You can multiply 'dir' by R_earth to keep the frequency
    // constant relative to the surface distance
    return dir * (R_earth + altitude);
}

// Maps Cartesian coordinates to a flattened UV-style projection
// useful if your noise textures are authored for a flat plane
vec2 getSphericalUV(vec3 p, vec3 earthCenter) {
    vec3 dir = normalize(p - earthCenter);
    return vec2(
        atan(dir.z, dir.x) / (2.0 * PI),
        asin(dir.y) / PI + 0.5
    );
}

vec3 getUnstretchedCoords(vec3 p, vec3 earthCenter, vec3 viewPos, float R_earth) {
    float altitude = length(p - earthCenter) - R_earth;
    vec3 P_norm = normalize(p - earthCenter);

    // Calculate the angle from the zenith (where zenith is vec3(0,1,0) at the viewer)
    float theta = acos(clamp(P_norm.y, -1.0, 1.0));
    float arc_dist = R_earth * theta;

    // Find the horizontal direction
    vec2 p_xz = vec2(P_norm.x, P_norm.z);
    float len_xz = length(p_xz);
    vec2 dir_XZ = p_xz / (len_xz + 1e-6); // Branchless normalize

    // Reconstruct a vector where Y is strictly altitude,
    // and X/Z are un-stretched arc lengths from the viewer.
    return vec3(viewPos.x + dir_XZ.x * arc_dist, altitude, viewPos.z + dir_XZ.y * arc_dist);
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
	float R_floor = R_earth + (cloudAltitude - 200.0) * worldScale;
	float R_ceiling = R_earth + (cloudAltitude + cloudThickness + 2000.0) * worldScale;

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
	float avgCloudDist = 0.0;
	float totalWeight = 0.0;

	if (t_start < t_end) {
		vec3 lightEnergy = vec3(0.0);

		int samples = 48;
		int shadow_samples = 4;

		// Capture primary light direction for multi-direction ambient sampling
		vec3 primaryLightDir = vec3(0, 1, 0);
		for (int j = 0; j < num_lights; j++) {
			if (lights[j].type == LIGHT_TYPE_DIRECTIONAL) {
				primaryLightDir = normalize(-lights[j].direction);
				break;
			}
		}

		float jitter = fastSpatiotemporalBlueNoise(TexCoords, 0, int(10*time));
		float stepSize = (t_end - t_start) / float(samples);

		for (int i = 0; i < samples; i++) {
			float t = t_start + (float(i) + jitter) * stepSize;
			if (t > dist)
				break;

			vec3 p = viewPos + rayDir * t;
			float altitude = length(p - earthCenter) - R_earth;
			p.y = altitude;

			float h_norm = clamp((altitude - props.altitude * props.worldScale) / max(props.thickness * props.worldScale, 1.0), 0.0, 1.0);

			CloudWeather weather = computeCloudWeather(p, props);
			CloudLayer layer = computeCloudLayer(weather, props);

			float d = calculateCloudDensity(p, weather, layer, props, time, false);
			if (d <= 0.01)
				continue;

			float stepDensity = d * stepSize * 0.005;
			float transmittanceAtStep = exp(-stepDensity);

			vec3 stepScattering = vec3(0.0);
			for (int j = 0; j < num_lights; j++) {
				if (lights[j].type != LIGHT_TYPE_DIRECTIONAL) {
					continue;
				}

				vec3 L = normalize(-lights[j].direction);

				vec3  voxelToCenter = p - earthCenter;
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

				float maxShadowDist = layer.thickness * 1.5;
				float traceDist = min(st_end - st_start, maxShadowDist);
				float shadowStepSize = traceDist / float(shadow_samples);
				// float shadowStepSize = (st_end - st_start) / float(shadow_samples);
				for (int k = 0; k < shadow_samples; k++) {
					vec3 sp = p + L * (float(k) + 0.5) * shadowStepSize;
					vec3 sp_curved = sp;
					sp_curved.y = length(sp - earthCenter) - R_earth;

					shadowDensity += calculateCloudDensity(sp_curved, weather, layer, props, time, true);
				}
				float opticalDepthToLight = shadowDensity * shadowStepSize * cloudShadowOpticalDepthMultiplier;
				float shadowTerm = mix(
					// beerPowder(opticalDepthToLight, d),
					beerPowder(opticalDepthToLight, stepDensity),
					exp(-opticalDepthToLight),
					cloudBeerPowderMix
				);

				// stepScattering += sunTransmittance * lights[j].color * shadowTerm * phase * lights[j].intensity * (j
				// == 0 ? cloudSunLightScale : cloudMoonLightScale);
				stepScattering += sunTransmittance * shadowTerm * phase * lights[j].intensity *
					(j == 0 ? cloudSunLightScale : cloudMoonLightScale);
			}

			// Multi-direction SH ambient: blend overhead sky with sun-facing horizon.
			// At sunset the horizon sample carries warm orange/red tones.
			// Scale ambient down at low sun angles so warm direct light dominates.
			vec3  ambientUp = evalSHIrradiance(vec3(0, 1, 0));
			vec3  ambientHorizon = evalSHIrradiance(normalize(vec3(primaryLightDir.x, 0.15, primaryLightDir.z)));
			float sunHeight = max(primaryLightDir.y, 0.0);
			float ambientScale = mix(0.3, 1.0, smoothstep(0.0, 0.3, sunHeight));
			vec3  ambient = mix(ambientUp, ambientHorizon, 0.4) * ambientScale;
			vec3  S = (stepScattering + ambient*smoothstep(0, 1, h_norm));

			// lightEnergy += cloudTransmittance * S * stepDensity;
			float weight = cloudTransmittance * (1.0 - transmittanceAtStep);
			lightEnergy += S * weight;
			avgCloudDist += t * weight;
			totalWeight += weight;

			cloudTransmittance *= transmittanceAtStep;

			if (cloudTransmittance < 0.01) {
				break;
			}
		}

		cloudColor = lightEnergy; // * cloudColorUniform;
		if (totalWeight > 0.001) {
			avgCloudDist /= totalWeight;
		} else {
			avgCloudDist = 50000.0 * worldScale;
		}
	} else {
		avgCloudDist = 50000.0 * worldScale;
	}

	FragColor = vec4(cloudColor, cloudTransmittance);
	CloudDepth = avgCloudDist;
}
