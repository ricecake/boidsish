#version 430 core
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

uniform sampler2D u_transmittanceLUT;
uniform sampler3D u_aerialPerspectiveLUT;

#include "../atmosphere/common.glsl"
#include "../helpers/lighting.glsl"
#include "../helpers/fast_noise.glsl"

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


float remap(float value, float low1, float high1, float low2, float high2) {
	return low2 + (value - low1) * (high2 - low2) / max(0.0001, (high1 - low1));
}

// float rayleighPhase(float cosTheta) {
// 	return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
// }

float henyeyGreenstein(float g, float cosTheta) {
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

float cloudPhase(float cosTheta) {
	// Dual-lobe Henyey-Greenstein for forward and back scattering
	return mix(henyeyGreenstein(0.8, cosTheta), henyeyGreenstein(-0.2, cosTheta), 0.5);
}

float beerPowder(float d, float local_d) {
	return max(exp(-d), exp(-d * 0.5) * 0.7 * (1.0 - exp(-local_d * 2.0)));
}

float getCloudDensity(vec3 p, float altitude, float thickness, bool simplified) {
	float h = (p.y - altitude) / max(thickness, 0.001);
	float tapering = smoothstep(0.0, 0.05, h) * smoothstep(1.0, 0.95, h);
	if (tapering <= 0.01) return 0.0;

	p += 2.0*fastCurl3d(vec3(p.xz/500.0, time/60.0));

	// Use multiple scales of noise for volumetric detail
	vec3 p_scaled = p / (1000.0 * worldScale);
	float base = fastWorley3d(vec3(p_scaled.xz, p_scaled.y+time * 0.002));
	base = remap(base, -1.0, 1.0, 0.0, 1.0);

	if (simplified) {
		return smoothstep(0.4, 0.8, base) * cloudDensity * tapering;
	}

	float detail = fastWarpedFbm3d(vec3(p.xz / (500.0 * worldScale), p.y / (40.0 * worldScale) + time * 0.05));
	detail = detail * 0.5 + 0.5;

	float noise = remap(base, detail * 0.3, 1.0, 0.0, 1.0);

	// Higher frequency erosion for "fluffy" edges
	float erosion = fastSimplex3d(p / (250.0 * worldScale)) * 0.5 + 0.5;
	noise = noise * mix(1.0, erosion, 0.4);

	return smoothstep(0.4, 0.7, noise) * cloudDensity * tapering;
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
		dist = 1000.0 * worldScale;
		worldPos = cameraPos + rayDir * dist;
	}

	// Use hazeDensity to scale the effective distance in the atmosphere
	float distKM = (dist / 1000.0) * (hazeDensity * 300.0);
	vec3  inScattering = sampleAerialPerspective(rayDir, distKM);
	float transmittance = sampleAerialPerspectiveTransmittance(rayDir, distKM);

	// 2. Cloud Layer (Simplified but preserved)
	vec3  cloudColor = vec3(0.0);

	float scaledCloudAltitude = cloudAltitude * worldScale;
	float scaledCloudThickness = (150+cloudThickness) * worldScale;

	float t_start = (scaledCloudAltitude - cameraPos.y) / (rayDir.y + 0.000001);
	float t_end = (scaledCloudAltitude + scaledCloudThickness - cameraPos.y) / (rayDir.y + 0.000001);

	if (t_start > t_end) {
		float temp = t_start;
		t_start = t_end;
		t_end = temp;
	}
	t_start = max(t_start, 0.0);
	t_end = min(t_end, dist);

	float cloudTransmittance = 1.0;
	if (t_start < t_end) {
		vec3  lightEnergy = vec3(0.0);
		int   samples = 16;
		float stepSize = (t_end - t_start) / float(samples);
		float jitter = fastBlueNoise2d(TexCoords * 10.0 + vec2(sin(time * 0.07), cos(time * -0.05)));

		for (int i = 0; i < samples; i++) {
			float t = mix(t_start, t_end, (float(i) + jitter) / float(samples));
			vec3  p = cameraPos + rayDir * t;
			float d = getCloudDensity(p, scaledCloudAltitude, scaledCloudThickness, false);

			if (d > 0.01) {
				float stepDensity = d * stepSize * 0.01;
				float transmittanceAtStep = exp(-stepDensity);

				vec3 stepScattering = vec3(0.0);
				for (int j = 0; j < min(num_lights, 4); j++) {
					vec3  L = (lights[j].type == 1) ? normalize(-lights[j].direction) : normalize(lights[j].position - p);
					float cosTheta = dot(rayDir, L);
					float phase = (j == 0) ? rayleighPhase(cosTheta)+cloudPhase(cosTheta) : 1.0 / (4.0 * PI);

					float shadowTerm = 1.0;
					if (j == 0) {
						// Shadow march only for the primary light
						float shadowDensity = 0.0;
						int   shadowSamples = 2;
						float shadowStepSize = scaledCloudThickness / float(shadowSamples) * 0.5;
						for (int k = 0; k < shadowSamples; k++) {
							vec3 sp = p + L * (float(k) + 0.5) * shadowStepSize;
							shadowDensity += getCloudDensity(sp, scaledCloudAltitude, scaledCloudThickness, true);
						}
						float opticalDepthToLight = shadowDensity * shadowStepSize * 0.02;
						shadowTerm += beerPowder(opticalDepthToLight, d);
					}

					stepScattering += lights[j].color * lights[j].intensity * shadowTerm * phase;
				}

				vec3 S = stepScattering + (ambient_light * 0.1);
				lightEnergy += cloudTransmittance * S * stepDensity;
				cloudTransmittance *= transmittanceAtStep;

				if (cloudTransmittance < 0.01) break;
			}
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
