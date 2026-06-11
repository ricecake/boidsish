#version 460 core

#include "lighting.glsl"
#include "particle_types.glsl"
#include "atmosphere/common.glsl"

in float         v_lifetime;
in vec4          view_pos;
in vec4          v_pos;
in vec3          v_vel;
in vec3          v_vel_view;
in vec3          v_origin;
flat in int      v_style;
flat in int      v_emitter_index;
flat in int      v_emitter_id;
flat in uint     v_particle_idx;
out vec4         FragColor;
flat in Particle v_p;

uniform float u_time;
uniform vec3  u_biomeAlbedos[8];
#include "helpers/fast_noise.glsl"
#include "helpers/noise.glsl"

// Robust polynomial fit for HDR-friendly fire
vec3 blackbody_hdr(float t) {
	vec3 col;
	// Red kicks in immediately and saturates quickly
	col.r = smoothstep(0.0, 0.2, t);

	// Green starts earlier for more orange and yellow
	col.g = smoothstep(0.02, 0.4, t);

	// Blue for the core hot spot
	col.b = smoothstep(0.3, 0.8, t);

	// Hollywood stunt fire: Rich orange/yellow boost
	return col * vec3(8.0, 2.5, 1.5);
}

float turbulence(vec2 p) {
	return fastRidge3d(vec3(p, u_time));
}

const float kExhaustLifetime = 2.0;
const float kExplosionLifetime = 2.5;
const float kFireLifetime = 5.0;
const float kSparksLifetime = 0.8;
const float kGlitterLifetime = 3.5;

void main() {
	// Shape the point into a circle and discard fragments outside the circle
	vec2  circ = gl_PointCoord - vec2(0.5);
	float distSq = dot(circ, circ);
	float shapeMask = smoothstep(0.25, 0.1, distSq);

	vec3  color = vec3(0.0);
	float alpha = 0.0;

	if (v_style == STYLE_ROCKET_TRAIL || v_style == STYLE_SPARKS || v_style == STYLE_GLITTER || v_style == STYLE_BUBBLES || v_style == STYLE_FIREFLIES || v_style == STYLE_DEBUG ||
	    v_style == STYLE_CINDER || v_style == STYLE_IRIDESCENT || v_style == STYLE_RAIN || v_style == STYLE_SNOW || v_style == STYLE_LEAF || v_style == STYLE_PETAL || v_style == STYLE_BIRDS || v_style == STYLE_FAIRY || v_style == STYLE_DUST) {

		color = v_p.color.rgb;
		alpha = v_p.color.a;

		if (v_style == STYLE_LEAF || v_style == STYLE_PETAL || v_style == STYLE_FIREFLIES || v_style == STYLE_BIRDS || v_style == STYLE_FAIRY) {
			vec3 biome_albedo = (v_emitter_index >= 0 && v_emitter_index < 8) ? u_biomeAlbedos[v_emitter_index] : vec3(0.5);
			color = mix(color, biome_albedo, 0.5);
		}

		if (v_style == STYLE_BUBBLES) {
			vec3 n; n.xy = circ * 2.0;
			float magSq = dot(n.xy, n.xy);
			n.z = sqrt(max(0.0, 1.0 - magSq));
			float fresnel = pow(max(0.0, 1.0 - n.z), 3.0);
			float swirl = sin(v_lifetime * 2.0 + gl_PointCoord.y * 5.0) * 0.5 + 0.5;
			vec3 iridescent_color = vec3(sin(swirl * 5.0) * 0.5 + 0.5, sin(swirl * 5.0 + 2.0) * 0.5 + 0.5, sin(swirl * 5.0 + 4.0) * 0.5 + 0.5);
			vec3 l = normalize(vec3(0.5, 0.5, 1.0));
			vec3 h = normalize(l + vec3(0, 0, 1));
			float spec = pow(max(dot(n, h), 0.0), 64.0);
			color = mix(iridescent_color, vec3(1.0), fresnel * 0.5 + 0.2) + spec;
		} else if (v_style == STYLE_SNOW) {
			float r = 0.5 * length(circ);
			float a = atan(circ.y, circ.x);
			float s = abs(sin(a * 3.0));
			shapeMask = smoothstep(0.1, 0.09, r * s);
		} else if (v_style == STYLE_RAIN) {
			vec2 vel_dir = normalize(v_vel_view.xy + vec2(1e-6));
			float angle = atan(vel_dir.y, vel_dir.x) + 1.5707;
			mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
			vec2 uv = (gl_PointCoord - 0.5) * rot + 0.5;
			float y = clamp(uv.y, 0.0, 1.0);
			float width = mix(0.02, 0.15, y);
			float streak = smoothstep(width*0.25, width * 0.05, abs(uv.x - 0.5)) * smoothstep(0.0, 0.2, uv.y) * smoothstep(1.0, 0.8, uv.y);
			alpha *= streak;
			color = vec3(0.2, 0.3, 0.5);
			shapeMask = 1.0;
		} else if (v_style == STYLE_IRIDESCENT) {
			float fresnel = pow(max(0.0, 1.0 - distSq * 4.0), 5.0);
			float angle_factor = pow(clamp(1.0 - distSq * 4.0, 0.0, 1.0), 2.0);
			float swirl = sin(v_lifetime * 0.5 + gl_PointCoord.y * 2.0) * 0.5 + 0.5;
			vec3 iridescent_color = vec3(sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5, sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5, sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5);
			vec3 view_dir = length(view_pos.xyz) > 0.001 ? normalize(-view_pos.xyz) : vec3(0, 0, 1);
			vec3 r = reflect(-view_dir, vec3(0, 1, 0));
			float spec = pow(max(dot(view_dir, r), 0.0), 64.0);
			color = mix(iridescent_color, vec3(1.0), fresnel) + 1.5 * spec * vec3(1.0);
		} else if (v_style == STYLE_CINDER) {
			float n = snoise3d(vec3(gl_PointCoord * 6.0, float(v_particle_idx)));
			shapeMask = smoothstep(0.2 + n * 0.15, 0.05, distSq);
		} else if (v_style == STYLE_DUST) {
			float distToCam = length(view_pos.xyz);
			shapeMask = exp(-distSq * 15.0);
			color = mix(vec3(1.5), mix(vec3(0.5, 0.8, 0.3), 2*vec3(2,1.2,0.4), smoothstep(316, 320, v_p.phase)), smoothstep(270.0, 280.0, v_p.phase));

			// Match new K_ENV_QUEUE_RADIUS (60.0)
			float boundaryFade = 1.0 - smoothstep(K_ENV_QUEUE_RADIUS - 10.0, K_ENV_QUEUE_RADIUS, distToCam);
			alpha *= boundaryFade * smoothstep(0.0, 0.5, v_lifetime);
			alpha = clamp(alpha, 0.0, 0.5);
		} else if (v_style == STYLE_BIRDS) {
			float flap = sin(u_time * 15.0 + v_p.phase);
			float wing_y = abs(gl_PointCoord.x - 0.5) * (0.5 + flap * 0.4);
			float body = (1-smoothstep(0.0, 0.1, abs(gl_PointCoord.y - 0.5 - wing_y)) + abs(gl_PointCoord.x - 0.7) * 0.5);
			body *= step(0.70, body);
			shapeMask = body;
			color = mix(
				mix(color, vec3(0.5, 0.8, 0.3), 0.43),
				mix(color * 2.0, vec3(0.2, 0.9, 0.9), 0.63),
				smoothstep(0.30, 0.40, gl_PointCoord.x) * (1.0 - smoothstep(0.60, 0.70, gl_PointCoord.x))
			);
			alpha = 1.0;
		} else if (v_style == STYLE_FAIRY) {
			// Phase-based brightness (using p.counter and p.phase logic from firefly)
			float twinkle = pow(smoothstep(0.0, 0.3, v_p.counter) * (1.0 - smoothstep(0.4, 0.6, v_p.counter)), 2.0) * step(v_p.counter, 0.6);

			float waver = 1.0 + 0.2 * snoise(vec2(u_time * 40.0, float(v_particle_idx) * 0.13));
			float flicker = mix(1.0, waver, smoothstep(0.8, 1.0, twinkle));

			float brightness = (0.5 + twinkle * 10.0) * flicker;

			// Small soft core
			float core = exp(-distSq * 100.0);
			// Large penumbra
			float penumbra = exp(-distSq * 5.0);

			// Iridescent sheen in the penumbra
			float angle_factor = pow(clamp(1.0 - distSq * 4.0, 0.0, 1.0), 2.0);
			vec3 iridescent_color = vec3(
				sin(angle_factor * 8.0 + v_p.phase) * 0.5 + 0.5,
				sin(angle_factor * 8.0 + v_p.phase + 2.0) * 0.5 + 0.5,
				sin(angle_factor * 8.0 + v_p.phase + 4.0) * 0.5 + 0.5
			);

			// Glittering sparkles using Worley noise
			float sparkle = pow(fastWorley3d(v_pos.xyz * 10.0 + u_time * 2.0), 6.0);
			vec3 sparkle_color = vec3(1.0, 0.9, 0.6) * sparkle * 10.0;

			color = mix(vec3(1.0, 1.0, 1.0), iridescent_color, 0.7) * penumbra;
			color += core * 2.0;
			color += sparkle_color * penumbra;
			color *= brightness;

			shapeMask = penumbra;
			alpha = (shapeMask  + sparkle ) * (twinkle);
		}

		alpha *= shapeMask;
		color *= alpha;
	} else {
		float maxLife = (v_style == STYLE_EXPLOSION) ? kExplosionLifetime : kFireLifetime;
		float distFromEpicenter = length(v_pos.xyz - v_origin);
		float normalizedLife = clamp(v_lifetime / maxLife, 0.0, 1.0);
		float roilScale = (v_style == STYLE_EXPLOSION) ? 0.015 : 0.03;
		float roil = fastFbm3d(v_pos.xyz * roilScale - vec3(0.0, u_time * 0.1, 0.0)) * 0.5 + 0.5;
		float worleyScale = (v_style == STYLE_EXPLOSION) ? 0.05 : 0.1;
		float knobly = fastWorley3d(v_pos.xyz * worleyScale * (1.0 + distFromEpicenter * 0.03) + vec3(u_time * 0.05));
		float noiseDetail = mix(roil, knobly, abs(fastSimplex3d(vec3(0, u_time, 0))));
		noiseDetail = mix(noiseDetail, noiseDetail * (fastSimplex3d(vec3(gl_PointCoord * 0.4, u_time * 0.05)) * 0.5 + 0.5), 0.35);
		float heat = normalizedLife * pow(noiseDetail, 1.4) * pow(max(0.0, 1.0 - (distSq * 4.0)), 0.7) * ((v_style == STYLE_EXPLOSION) ? smoothstep(80.0, 0.0, distFromEpicenter) : 1.0);
		alpha = shapeMask * smoothstep(0.01, 0.12, heat) * turbulence(gl_PointCoord);
		color = blackbody_hdr(heat) * alpha * 12.0 * (1.0 + normalizedLife);
	}

	// Apply atmospheric scattering and fog to particles
	// Note: v_pos.xyz is world position
	float depth = length(view_pos.xyz);

	float transmittance = 1.0;
	vec3 scattering = vec3(1.0);

#ifdef ATMOSPHERE_COMMON_GLSL
	// Calculate atmosphere properties at particle position
	Sampling s = getAtmospherePropertiesAtPos(v_pos.xyz);

	// Physically-based fogging:
	// 1. Transmittance attenuates the particle's own emission
	// 2. Scattering adds the atmosphere's own glow between camera and particle
	transmittance = exp(-length(s.extinction) * (depth / 1000.0)); // Convert meters to KM for extinction lookup
	scattering = ambient_light * (1.0 - transmittance);
	color = color * transmittance;
#endif

	// Dual exposure/lighting fix:
	// Ambient particles get standard lighting, while emissive ones get a boost.
	if (v_style == STYLE_ROCKET_TRAIL || v_style == STYLE_FIRE || v_style == STYLE_EXPLOSION || v_style == STYLE_SPARKS || v_style == STYLE_GLITTER || v_style == STYLE_FIREFLIES) {
		// Emissive/self-lit particles are already bright enough.
		// For fire (additive), we only apply transmittance to the color.
		// We don't add ambient scattering directly as it would make the fire look like a solid block in fog.
		// Instead, we let the scattering affect the scene behind it.
	} else {
		// Ambient particles (leaves, petals, birds, etc.) should receive scene ambient.
		vec3 ambient = sh_coeffs[0].xyz * 0.5 + 0.5; // Simple approximation of global ambient
		color *= ambient * (1.0 + nightFactor);
	}

	FragColor = vec4(color, alpha);
}
