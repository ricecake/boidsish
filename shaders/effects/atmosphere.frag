#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;

// uniform vec3 cameraPos; // Use viewPos from Lighting UBO
uniform mat4 invView;
uniform mat4 invProjection;

// Atmosphere settings (now pulled from Config in C++, but some still passed as uniforms for clouds)
uniform float cloudDensity;
uniform float cloudAltitude;
uniform float cloudThickness;
uniform vec3  cloudColorUniform;

uniform bool enableClouds;
uniform bool enableFog;

#include "../helpers/lighting.glsl"
#include "../helpers/noise.glsl"
#include "../atmosphere/common.glsl"

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
		// We use a vertical-only offset for ro to ensure horizontal invariance
		vec3 ro = vec3(0.0, viewPos.y + bottomRadius, 0.0);
		const int SAMPLES = 32;
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
				vec3 L;
				if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
					L = normalize(-lights[i].direction);
				} else {
					L = normalize(lights[i].position - intersect);
				}

				float d = max(0.0, dot(vec3(0, 1, 0), L));
				float silver = pow(max(0.0, dot(rayDir, L)), 4.0) * 0.5;

				// Cloud intensity also benefits from the sunIntensityFactor for cinematic look
				float intens = lights[i].intensity;
				if (i == 0) intens *= sunIntensityFactor * 0.1; // Scale down factor for clouds to avoid over-exposure

				cloudScattering += lights[i].color * (d * 0.5 + 0.5 + silver) * intens;
			}

			cloudColor = cloudColorUniform * (ambient_light + cloudScattering * 0.5);
			result = mix(result, cloudColor, cloudFactor);
		}
	}

	FragColor = vec4(result, 1.0);
}
