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

uniform sampler2D u_transmittanceLUT;
uniform sampler3D u_aerialPerspectiveLUT;

#include "../helpers/lighting.glsl"
#include "../helpers/noise.glsl"
#include "../atmosphere/common.glsl"

vec3 sampleAerialPerspective(vec3 rd, float distKM) {
    float azimuth = atan(rd.x, -rd.z);
    if (azimuth < 0.0) azimuth += 2.0 * PI;
    float elevation = asin(clamp(rd.y, -1.0, 1.0));

    float u = azimuth / (2.0 * PI);
    float v = elevation / PI + 0.5;
    float w = distKM / 32.0; // maxDist in AP LUT

    return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).rgb;
}

float sampleAerialPerspectiveTransmittance(vec3 rd, float distKM) {
    float azimuth = atan(rd.x, -rd.z);
    if (azimuth < 0.0) azimuth += 2.0 * PI;
    float elevation = asin(clamp(rd.y, -1.0, 1.0));

    float u = azimuth / (2.0 * PI);
    float v = elevation / PI + 0.5;
    float w = distKM / 32.0;

    return texture(u_aerialPerspectiveLUT, vec3(u, v, w)).a;
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
    vec3 inScattering = sampleAerialPerspective(rayDir, distKM);
    float transmittance = sampleAerialPerspectiveTransmittance(rayDir, distKM);

	// 2. Cloud Layer (Simplified but preserved)
	float cloudFactor = 0.0;
	vec3  cloudColor = vec3(0.0);

	float scaledCloudAltitude = cloudAltitude * worldScale;
	float scaledCloudThickness = cloudThickness * worldScale;

	float t_start = (scaledCloudAltitude - cameraPos.y) / (rayDir.y + 0.000001);
	float t_end = (scaledCloudAltitude + scaledCloudThickness - cameraPos.y) / (rayDir.y + 0.000001);

	if (t_start > t_end) { float temp = t_start; t_start = t_end; t_end = temp; }
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
			float d = smoothstep(0.2, 0.6, noise * (i + (1 - noise))) * cloudDensity * tapering;
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
