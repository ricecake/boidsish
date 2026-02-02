#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

#include "helpers/atmosphere.glsl"

uniform mat4  invProjection;
uniform mat4  invView;
uniform float atmosphereExposure;

// --- Existing Noise Functions ---

vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(vec4 r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}

vec2 fade(vec2 t) {
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float snoise(vec3 v) {
	const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
	const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

	// First corner
	vec3 i = floor(v + dot(v, C.yyy));
	vec3 x0 = v - i + dot(i, C.xxx);

	// Other corners
	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min(g.xyz, l.zxy);
	vec3 i2 = max(g.xyz, l.zxy);

	vec3 x1 = x0 - i1 + C.xxx;
	vec3 x2 = x0 - i2 + C.yyy;
	vec3 x3 = x0 - D.yyy;

	// Permutations
	i = mod289(i);
	vec4 p = permute(
		permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x +
		vec4(0.0, i1.x, i2.x, 1.0)
	);

	float n_ = 0.142857142857;
	vec3  ns = n_ * D.wyz - D.xzx;

	vec4 j = p - 49.0 * floor(p * ns.z * ns.z);

	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_);

	vec4 x = x_ * ns.x + ns.yyyy;
	vec4 y = y_ * ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);

	vec4 b0 = vec4(x.xy, y.xy);
	vec4 b1 = vec4(x.zw, y.zw);

	vec4 s0 = floor(b0) * 2.0 + 1.0;
	vec4 s1 = floor(b1) * 2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));

	vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
	vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

	vec3 p0 = vec3(a0.xy, h.x);
	vec3 p1 = vec3(a0.zw, h.y);
	vec3 p2 = vec3(a1.xy, h.z);
	vec3 p3 = vec3(a1.zw, h.w);

	// Normalise gradients
	vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

	// Mix final noise value
	vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
	m = m * m;
	return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

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

float fbm_sky(vec3 p) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; i++) {
		value += amplitude * snoise(p);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

void main() {
	// Reconstruct world ray
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

	// Planet center offset: assume camera is near (0,0,0) and surface is at y=0
	vec3 ro = vec3(0.0, Re + max(1.0, viewPos.y), 0.0);

	float t0, t1;
	if (!intersectSphere(ro, world_ray, Ra, t0, t1) || t1 < 0.0) {
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	t0 = max(t0, 0.0);

	// Check if ray hits planet
	float tp0, tp1;
	bool  hitPlanet = intersectSphere(ro, world_ray, Re, tp0, tp1) && tp0 > 0.0;
	if (hitPlanet) {
		t1 = min(t1, tp0);
	}

	vec3 scattering = vec3(0.0);

	for (int i = 0; i < num_lights; i++) {
		if (lights[i].type == 1) { // DIRECTIONAL_LIGHT
			vec3 lightDir = normalize(-lights[i].direction);
			vec3 lightColor = lights[i].color * lights[i].intensity;
			scattering += calculateScattering(ro, world_ray, t1 - t0, i, 12);

			// Sun disc
			float cosTheta = dot(world_ray, lightDir);
			float sun = smoothstep(0.9995, 0.9998, cosTheta);
			if (sun > 0.0 && !hitPlanet) {
				float st0, st1;
				float stp0, stp1;
				bool  sunBlockedPlanet = intersectSphere(ro, world_ray, Re, stp0, stp1) && stp0 > 0.0;

				if (!sunBlockedPlanet && intersectSphere(ro, world_ray, Ra, st0, st1)) {
					// Check terrain occlusion for sun disc
					float shadow = calculateShadow(i, viewPos, lightDir, lightDir);

					vec2 odSun = opticalDepth(ro, world_ray, st1, 4);
					vec3 sunAtten = exp(-(betaR * odSun.x + betaM * 1.1 * odSun.y));
					scattering += sun * lightColor * sunAtten * 10.0 * shadow;
				}
			}
		}
	}

	// Calculate view transmittance for background elements
	vec2 odViewTotal = opticalDepth(ro, world_ray, t1 - t0, 12);
	vec3 transmittance = exp(-(betaR * odViewTotal.x + betaM * 1.1 * odViewTotal.y));

	// Background Layer (Stars and Nebula)
	vec3 background_color = vec3(0.0);
	if (!hitPlanet) { // Only if we are looking at the sky, not the ground
		// Nebula/Haze
		vec3  p_noise = world_ray * 4.0;
		vec3  warp_offset = vec3(fbm_sky(p_noise + time * 0.05));
		float nebula_noise = fbm_sky(p_noise + warp_offset * 0.5);
		vec3  nebula_palette = mix(vec3(0.0, 0.1, 0.4), vec3(0.8, 0.2, 0.7), nebula_noise);
		background_color += nebula_palette * 0.4;

		// Stars
		float stars = starLayer(world_ray);
		background_color += stars * vec3(1.0, 0.9, 0.8);
	}

	vec3 final_color = (scattering + background_color * transmittance) * atmosphereExposure;

	// HDR Tone Mapping (simple Reinhard)
	final_color = final_color / (final_color + vec3(1.0));
	// Gamma correction
	final_color = pow(final_color, vec3(1.0 / 2.2));

	FragColor = vec4(final_color, 1.0);
}
