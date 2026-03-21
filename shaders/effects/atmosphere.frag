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
#include "../helpers/fast_noise.glsl"
#include "../helpers/lighting.glsl"

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
	float cloudFactor = 0.0;
	vec3  cloudColor = vec3(0.0);

	float scaledCloudAltitude = cloudAltitude * worldScale;
	float t_start = (scaledCloudAltitude - cameraPos.y) / (rayDir.y + 0.000001);

	vec3  cloudPoint = cameraPos + rayDir * t_start;
	float weatherMap = fastWorley3d(vec3(worldPos.xz / (4000 * worldScale), time * 0.01));

	float weatherThickness = max(20 * weatherMap + cloudThickness, cloudThickness);
	float scaledCloudThickness = weatherThickness * worldScale;
	float t_end = (scaledCloudAltitude + scaledCloudThickness - cameraPos.y) / (rayDir.y + 0.000001);

	float workingCloudDensity = cloudDensity + 5 * weatherMap;

	if (t_start > t_end) {
		float temp = t_start;
		t_start = t_end;
		t_end = temp;
	}
	t_start = max(t_start, 0.0);
	t_end = min(t_end, dist);

	if (t_start < t_end) {
		float cloudAcc = 0.0;
		int   samples = 4 * int(weatherThickness / cloudThickness);
		float jitter = fastBlueNoise(TexCoords * 10.0 + vec2(sin(time * 0.07), cos(time * -0.05)));
		for (int i = 0; i < samples; i++) {
			float t = mix(t_start, t_end, (float(i) + jitter) / float(samples));
			vec3  p = cameraPos + rayDir * t;
			float h = (p.y - scaledCloudAltitude) / max(100 + scaledCloudThickness, 0.001);
			float tapering = smoothstep(0.0, 0.05, h) * smoothstep(1.0, 0.95, h);
			if (tapering <= 0.1)
				continue;

			p += 2.0 * fastCurl3d(vec3(p.xz / 500.0, time / 60.0));
			vec3 p_scaled = p / (1000.0 * worldScale);

			float noise = fastWorley3d(vec3(p_scaled.xz, p_scaled.y + time * 0.01));
			float erosion = fastRidge3d(p / (600.0 * worldScale)) * 0.5 + 0.5;
			noise = remap(noise, 1 - erosion, 1.0, 0.0, 1.0);

			float d = smoothstep(0.2, 0.6, noise) * (workingCloudDensity)*tapering;
			cloudAcc += d;
		}
		cloudFactor = 1.0 - exp(-cloudAcc * (t_end - t_start) * 0.05 / float(samples));
		vec3 intersect = cameraPos + rayDir * mix(t_start, t_end, 0.5);
		vec3 cloudScattering = vec3(0.0);
		for (int i = 0; i < num_lights; i++) {
			vec3  L = normalize(lights[i].position - intersect);
			float d = max(0.0, dot(vec3(0, 1, 0), L));
			float silver = pow(max(0.0, dot(rayDir, L)), 4.0) * 0.5;
			cloudScattering += lights[i].color * (d * 0.5 + 0.5 + silver) * lights[i].intensity;
		}
		cloudColor = cloudColorUniform * (ambient_light + cloudScattering * 0.5);
	}

	// Combine everything
	vec3 result = mix(sceneColor, cloudColor, cloudFactor);

	// Apply physically accurate atmosphere
	if (depth < 1.0) {
		result = result * transmittance + inScattering;
	}

	FragColor = vec4(result, 1.0);
}
