#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;

// uniform vec3 cameraPos; // Use viewPos from Lighting UBO
uniform mat4 invView;
uniform mat4 invProjection;

uniform float hazeDensity;
uniform float hazeHeight;
uniform vec3  hazeColor;
uniform float cloudDensity;
uniform float cloudAltitude;
uniform float cloudThickness;
uniform vec3  cloudColorUniform;
// uniform float time; // Use time from Lighting UBO

uniform bool enableClouds;
uniform bool enableFog;

uniform sampler2D transmittanceLUT;
uniform sampler2D multiScatteringLUT;
uniform float sunIntensity;

#include "../helpers/lighting.glsl"
#include "../helpers/noise.glsl"
#include "../atmosphere/common.glsl"

vec3 get_transmittance(float r, float mu) {
	vec2 uv = transmittance_to_uv(r, mu);
	return texture(transmittanceLUT, uv).rgb;
}

vec3 get_scattering(vec3 p, vec3 rd, vec3 sun_dir, out vec3 transmittance) {
	float r = length(p);
	float mu = dot(p, rd) / max(r, 0.01);
	float mu_s = dot(p, sun_dir) / max(r, 0.01);
	float cos_theta = dot(rd, sun_dir);

	transmittance = get_transmittance(r, mu);
	vec3 trans_sun = get_transmittance(r, mu_s);

	float r_h = exp(-(r - bottomRadius) / rayleighScaleHeight);
	float m_h = exp(-(r - bottomRadius) / mieScaleHeight);

	vec3  rayleigh_scat = rayleighScattering * r_h * rayleigh_phase(cos_theta);
	vec3  mie_scat = vec3(mieScattering) * m_h * henyey_greenstein(cos_theta, mieAnisotropy);

	vec3  scat = (rayleigh_scat + mie_scat) * trans_sun * sunIntensity;

	// Add multi-scattering
	vec2 ms_uv = vec2(mu_s * 0.5 + 0.5, (r - bottomRadius) / (topRadius - bottomRadius));
	vec3 ms = texture(multiScatteringLUT, ms_uv).rgb;
	scat += ms * sunIntensity;

	return scat;
}

float fbm_clouds(vec2 p) {
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

	vec3  diff = worldPos - viewPos;
	float dist = length(diff);
	vec3  rayDir = diff / max(dist, 0.0001);

	if (depth == 1.0) {
		dist = 1000.0 * worldScale; // Assume sky is far
		worldPos = viewPos + rayDir * dist;
	}

	vec3 result = sceneColor;

	// 1. Atmosphere Scattering (Fog/Haze)
	// Skip for sky pixels (depth == 1.0) to avoid double-fogging, as sky.frag already handles it
	if (enableFog && depth < 1.0) {
		// We use a simplified version of the scattering for fog
		vec3 sun_dir;
		if (num_lights > 0) {
			if (lights[0].type == LIGHT_TYPE_DIRECTIONAL) {
				sun_dir = normalize(-lights[0].direction);
			} else {
				sun_dir = normalize(lights[0].position - viewPos);
			}
		} else {
			sun_dir = vec3(0, 1, 0);
		}

		// Integrate along the ray
		vec3 ro = viewPos + vec3(0, bottomRadius, 0);
		const int SAMPLES = 16;
		vec3 scat_acc = vec3(0.0);
		vec3 trans_acc = vec3(1.0);
		float ds = dist / float(SAMPLES);

		for (int i = 0; i < SAMPLES; i++) {
			float t = (float(i) + 0.5) * ds;
			vec3  p = ro + rayDir * t;
			vec3  trans;
			vec3  scat = get_scattering(p, rayDir, sun_dir, trans);

			AtmosphereSample s = sample_atmosphere(length(p) - bottomRadius);
			scat_acc += trans_acc * scat * ds;
			trans_acc *= exp(-s.extinction * ds);
		}

		result = sceneColor * trans_acc + scat_acc;
	}

	// 2. Cloud Layer
	if (enableClouds) {
		float cloudFactor = 0.0;
		vec3  cloudColor = vec3(0.0);

		float scaledCloudAltitude = cloudAltitude * worldScale;
		float scaledCloudThickness = cloudThickness * worldScale;

		float t_start = (scaledCloudAltitude - viewPos.y) / (rayDir.y + 0.000001);
		float t_end = (scaledCloudAltitude + scaledCloudThickness - viewPos.y) / (rayDir.y + 0.000001);

		if (t_start > t_end) {
			float temp = t_start;
			t_start = t_end;
			t_end = temp;
		}

		t_start = max(t_start, 0.0);
		t_end = min(t_end, dist);

		if (t_start < t_end) {
			float cloudAcc = 0.0;
			int   samples = 6;
			float jitter = fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453);

			for (int i = 0; i < samples; i++) {
				float t = mix(t_start, t_end, (float(i) + jitter) / float(samples));
				vec3  p = viewPos + rayDir * t;
				float h = (p.y - scaledCloudAltitude) / max(scaledCloudThickness, 0.001);
				float tapering = smoothstep(0.0, 0.2, h) * smoothstep(1.0, 0.5, h);

				float noise = fbm_clouds((p.xz / worldScale) * 0.015 + jitter * time * 0.0001 + (p.y / worldScale) * 0.02);
				float d = smoothstep(0.2, 0.6, noise * (i + (1 - noise))) * cloudDensity * tapering;

				cloudAcc += d;
			}
			cloudFactor = 1.0 - exp(-cloudAcc * (t_end - t_start) * 0.05 / float(samples));

			vec3 intersect = viewPos + rayDir * mix(t_start, t_end, 0.5);
			vec3 cloudScattering = vec3(0.0);
			for (int i = 0; i < num_lights; i++) {
				vec3  L = normalize(lights[i].position - intersect);
				float d = max(0.0, dot(vec3(0, 1, 0), L));
				float silver = pow(max(0.0, dot(rayDir, L)), 4.0) * 0.5;

				cloudScattering += lights[i].color * (d * 0.5 + 0.5 + silver) * lights[i].intensity;
			}

			cloudColor = cloudColorUniform * (ambient_light + cloudScattering * 0.5);
			result = mix(result, cloudColor, cloudFactor);
		}
	}

	FragColor = vec4(result, 1.0);
}
