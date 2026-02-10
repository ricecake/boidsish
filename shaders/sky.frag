#version 420 core

out vec4 FragColor;

in vec2 TexCoords;

#include "helpers/lighting.glsl"
#include "helpers/noise.glsl"
#include "atmosphere/common.glsl"

uniform mat4 invProjection;
uniform mat4 invView;

uniform sampler2D transmittanceLUT;
uniform sampler2D multiScatteringLUT;
uniform float sunIntensity;

// Star field logic from original sky.frag
vec3 hash33(vec3 p) {
	p = fract(p * vec3(443.897, 441.423, 437.195));
	p += dot(p, p.yxz + 19.19);
	return fract((p.xxy + p.yxx) * p.zyx);
}

float starLayer(vec3 dir) {
	float scale = 100.0;
	vec3  id = floor(dir * scale);
	vec3  local_uv = fract(dir * scale);
	vec3  star_pos = hash33(id);
	float brightness = abs(sin(time / 2.0 + star_pos.x * 100.0));
	vec3  center = vec3(0.5) + (star_pos - 0.5) * 0.8;
	float dist = length(local_uv - center);
	float radius = 0.05 * brightness;
	return smoothstep(radius, radius * 0.5, dist);
}

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

	float r_h = exp(-max(0.0, r - bottomRadius) / max(0.01, rayleighScaleHeight));
	float m_h = exp(-max(0.0, r - bottomRadius) / max(0.01, mieScaleHeight));

	vec3  rayleigh_scat = rayleighScattering * r_h * rayleigh_phase(cos_theta);
	vec3  mie_scat = vec3(mieScattering) * m_h * henyey_greenstein(cos_theta, mieAnisotropy);

	vec3  scat = (rayleigh_scat + mie_scat) * trans_sun * sunIntensity;

	// Add multi-scattering
	vec2 ms_uv = vec2(mu_s * 0.5 + 0.5, (r - bottomRadius) / (topRadius - bottomRadius));
	vec3 ms = texture(multiScatteringLUT, ms_uv).rgb;
	scat += ms * sunIntensity;

	return scat;
}

void main() {
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

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

	// Atmosphere Scattering
	// We use a vertical-only offset for ro to ensure horizontal invariance
	vec3 ro = vec3(0.0, viewPos.y + bottomRadius, 0.0);
	float t0, t1;
	intersect_sphere(ro, world_ray, topRadius, t0, t1);
	float d = t1;

	// If hitting the ground, limit distance
	float tp0, tp1;
	if (intersect_sphere(ro, world_ray, bottomRadius, tp0, tp1)) {
		if (tp0 > 0.0) d = min(d, tp0);
	}

	const int SAMPLES = 16;
	vec3 scat_acc = vec3(0.0);
	vec3 trans_acc = vec3(1.0);
	float ds = d / float(SAMPLES);

	for (int i = 0; i < SAMPLES; i++) {
		float t = (float(i) + 0.5) * ds;
		vec3  p = ro + world_ray * t;
		vec3  trans;
		vec3  scat = get_scattering(p, world_ray, sun_dir, trans);

		AtmosphereSample s = sample_atmosphere(length(p) - bottomRadius);
		scat_acc += trans_acc * scat * ds;
		trans_acc *= exp(-s.extinction * ds);
	}

	vec3 final_color = scat_acc;

	// Solar Disc
	float sun_angular_radius = 0.015;
	float sun_cos_theta = dot(world_ray, sun_dir);

	// Distortion near horizon
	float flattening = 1.0 + 1.5 * smoothstep(0.1, -0.1, sun_dir.y);
	vec3 distorted_ray = world_ray;
	distorted_ray.y *= flattening;
	vec3 distorted_sun = sun_dir;
	distorted_sun.y *= flattening;

	float sun_dist = length(normalize(distorted_ray) - normalize(distorted_sun));
	float sun_disc = smoothstep(sun_angular_radius, sun_angular_radius * 0.9, sun_dist);

	if (sun_disc > 0.0) {
		vec3 sun_transmittance = get_transmittance(length(ro), sun_dir.y);
		final_color += sun_disc * sun_transmittance * sunIntensity * 2.0;
	}

	// Stars (only if sky is dark)
	float sky_brightness = dot(final_color, vec3(0.2126, 0.7152, 0.0722));
	float stars = starLayer(world_ray);
	final_color += stars * vec3(1.0, 0.9, 0.8) * smoothstep(0.1, 0.0, sky_brightness);

	FragColor = vec4(final_color, 1.0);
}
