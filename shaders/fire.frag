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

	if (v_style == STYLE_ROCKET_TRAIL || v_style == STYLE_SPARKS || v_style == STYLE_GLITTER || v_style == STYLE_BUBBLES || v_style == STYLE_DEBUG ||
	    v_style == STYLE_CINDER || v_style == STYLE_IRIDESCENT || v_style == STYLE_RAIN || v_style == STYLE_SNOW || v_style == STYLE_LEAF || v_style == STYLE_PETAL || v_style == STYLE_BIRDS || v_style == STYLE_FAIRY || v_style == STYLE_DUST || v_style == STYLE_FIREFLIES || v_style == STYLE_POOF || v_style == STYLE_BUTTERFLY) {

		color = v_p.color.rgb;
		alpha = v_p.color.a;

		if (v_style == STYLE_LEAF || v_style == STYLE_PETAL || v_style == STYLE_FAIRY || v_style == STYLE_FIREFLIES) {
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
			float distToCam = length(view_pos.xyz);
			float defocus = 1.0 - smoothstep(1.0, 6.0, distToCam);

			float r = 0.5 * length(circ);
			float a = atan(circ.y, circ.x);
			float s = abs(sin(a * 3.0));
			float sharpMask = smoothstep(0.1, 0.09, r * s);
			float bokehMask = smoothstep(0.25, 0.05, distSq);

			shapeMask = mix(sharpMask, bokehMask, defocus);
			alpha *= mix(1.0, 0.15, defocus);
		} else if (v_style == STYLE_RAIN) {
			float distToCam = length(view_pos.xyz);
			float defocus = 1.0 - smoothstep(1.0, 6.0, distToCam);

			vec2 vel_dir = normalize(v_vel_view.xy + vec2(1e-6));
			float angle = atan(vel_dir.y, vel_dir.x) + 1.5707;
			mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
			vec2 uv = (gl_PointCoord - 0.5) * rot + 0.5;
			float y = clamp(uv.y, 0.0, 1.0);
			float width = mix(0.02, mix(0.15, 0.45, defocus), y);
			float streak = smoothstep(width * 0.25, width * 0.05, abs(uv.x - 0.5)) * smoothstep(0.0, 0.2, uv.y) * smoothstep(1.0, 0.8, uv.y);
			alpha *= streak * mix(1.0, 0.15, defocus);
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
			float defocus = 1.0 - smoothstep(1.0, 6.0, distToCam);

			float sharpMask = exp(-distSq * 15.0);
			float bokehMask = exp(-distSq * 3.0);
			shapeMask = mix(sharpMask, bokehMask, defocus);
			color = mix(vec3(1.5), mix(vec3(0.5, 0.8, 0.3), 2*vec3(2,1.2,0.4), smoothstep(316, 320, v_p.phase)), smoothstep(270.0, 280.0, v_p.phase));

			// Match new K_ENV_QUEUE_RADIUS (60.0)
			float boundaryFade = 1.0 - smoothstep(K_ENV_QUEUE_RADIUS - 10.0, K_ENV_QUEUE_RADIUS, distToCam);
			alpha *= boundaryFade * smoothstep(0.0, 0.5, v_lifetime) * mix(1.0, 0.15, defocus);
			alpha = clamp(alpha, 0.0, 0.5);
		} else if (v_style == STYLE_BIRDS) {
			// Align bird billboard to its view-space velocity vector so it flies forward
			vec2 vel_dir = (length(v_vel_view.xy) > 0.05) ? normalize(v_vel_view.xy) : vec2(1.0, 0.0);
			// Birds fly heading in forward direction (upwards/along velocity)
			float angle = atan(vel_dir.y, vel_dir.x) - 1.5707;
			mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
			vec2 uv = rot * (gl_PointCoord - 0.5) + 0.5;

			// Realistic flapping cycle with arched wing profiles
			float flapPhase = v_p.phase;
			float wingFlap = sin(flapPhase);

			// Local UV centering: x in [-0.5, 0.5], y in [-0.5, 0.5]
			vec2 p = uv - vec2(0.5);
			float absX = abs(p.x);

			// Dynamic wing arching: upward stroke arches wings down, downward stroke arches wings up
			float wingArch = absX * (wingFlap * 0.8);
			// Wing chord thickness decreases towards wingtips
			float wingThickness = mix(0.14, 0.03, smoothstep(0.05, 0.45, absX));

			// Distance to wing curve centerline
			float wingDist = abs((p.y - wingArch) + 0.02);
			float wingMask = smoothstep(wingThickness, wingThickness - 0.03, wingDist) * smoothstep(0.48, 0.40, absX);

			// Slender bird body / head / tail line along y-axis
			float bodyWidth = mix(0.045, 0.01, smoothstep(0.0, 0.35, abs(p.y)));
			float bodyMask = smoothstep(bodyWidth, bodyWidth - 0.02, absX) * smoothstep(0.38, 0.30, abs(p.y));

			shapeMask = clamp(wingMask + bodyMask, 0.0, 1.0);

			// Peaceful feathered plumage shading (soft slate blue / warm underside accent)
			vec3 plumageDark = vec3(0.12, 0.16, 0.22);
			vec3 plumageHighlight = vec3(0.45, 0.55, 0.65);
			vec3 bellyAccent = vec3(0.85, 0.75, 0.60);

			float featherShade = smoothstep(0.0, 0.4, absX);
			vec3 birdColor = mix(plumageDark, plumageHighlight, featherShade);
			birdColor = mix(birdColor, bellyAccent, bodyMask * 0.5);

			color = birdColor;
			alpha = v_p.color.a * shapeMask;
		} else if (v_style == STYLE_BUTTERFLY) {
			// Align butterfly to velocity or upward float
			vec2 vel_dir = (length(v_vel_view.xy) > 0.02) ? normalize(v_vel_view.xy) : vec2(0.0, 1.0);
			float angle = atan(vel_dir.y, vel_dir.x) - 1.5707;
			mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
			vec2 uv = rot * (gl_PointCoord - 0.5) + 0.5;

			vec2 p = uv - vec2(0.5);
			float absX = abs(p.x);

			// 3D wing fold perspective modulation during flutter
			float flapPhase = v_p.phase;
			float foldFactor = cos(flapPhase) * 0.5 + 0.5; // [0, 1] fold
			float spanScale = mix(1.0, 0.25, foldFactor);  // Wing span compresses during flap

			// Un-fold local X to evaluate 2D wing silhouette
			float unFoldX = absX / max(0.1, spanScale);

			// Forewing (upper) and Hindwing (lower) shape evaluations
			float forewingRadius = 0.38;
			vec2 forewingCenter = vec2(0.18, 0.10);
			float foreDist = length((vec2(unFoldX, p.y) - forewingCenter) * vec2(1.0, 1.25));
			float foreMask = smoothstep(forewingRadius, forewingRadius - 0.04, foreDist);

			float hindwingRadius = 0.26;
			vec2 hindwingCenter = vec2(0.15, -0.12);
			float hindDist = length((vec2(unFoldX, p.y) - hindwingCenter) * vec2(1.1, 1.0));
			float hindMask = smoothstep(hindwingRadius, hindwingRadius - 0.04, hindDist);

			float wingSilhouette = clamp(foreMask + hindMask, 0.0, 1.0);

			// Delicate butterfly body & antennae
			float bodyWidth = mix(0.035, 0.01, smoothstep(0.0, 0.3, abs(p.y)));
			float bodyMask = smoothstep(bodyWidth, bodyWidth - 0.01, absX) * smoothstep(0.32, 0.25, abs(p.y));

			shapeMask = clamp(wingSilhouette + bodyMask, 0.0, 1.0);

			// Intricate wing margin, veins, and colorful iridescent glow
			float veinPattern = sin(unFoldX * 35.0 + p.y * 20.0) * 0.5 + 0.5;
			float borderMask = smoothstep(0.28, 0.36, length(vec2(unFoldX, p.y)));

			vec3 baseWingColor = v_p.color.rgb;
			vec3 borderDark = vec3(0.05, 0.05, 0.08);
			vec3 veinGlow = mix(baseWingColor, vec3(1.0, 0.95, 0.8), 0.35);

			vec3 finalButterflyColor = mix(baseWingColor, veinGlow, veinPattern * 0.3);
			finalButterflyColor = mix(finalButterflyColor, borderDark, borderMask * 0.8);
			finalButterflyColor = mix(finalButterflyColor, vec3(0.08), bodyMask);

			color = finalButterflyColor;
			alpha = v_p.color.a * shapeMask;
		} else if (v_style == STYLE_FAIRY) {
			float dist = length(circ);
			float twinkle = clamp((v_p.color.a / smoothstep(0.0, 0.5, v_lifetime)) - 0.2, 0.0, 1.0);

			float core = 1.0 - smoothstep(0.0, 0.15, dist);
			float penumbra = exp(-distSq * 5.0);

			// Expanding ring with wavering size
			float waver = snoise3d(vec3(v_pos.xyz * 0.5 + u_time * 2.0)) * 0.05 * twinkle;
			float ringRadius = 0.1 + twinkle * 0.35 + waver;
			float ringWidth = 0.02 + twinkle * 0.1;
			float ring = smoothstep(ringRadius + ringWidth, ringRadius, dist) * smoothstep(ringRadius - ringWidth, ringRadius, dist);

			shapeMask = max(max(core, ring), penumbra);

			// Iridescent sheen in the penumbra
			float angle_factor = pow(clamp(1.0 - distSq * 4.0, 0.0, 1.0), 2.0);
			vec3 iridescent_color = vec3(
				sin(angle_factor * 8.0 + v_p.phase) * 0.5 + 0.5,
				sin(angle_factor * 8.0 + v_p.phase + 2.0) * 0.5 + 0.5,
				sin(angle_factor * 8.0 + v_p.phase + 4.0) * 0.5 + 0.5
			);

			// Diffracted, gem-like sparkle in the ring
			float angle = atan(circ.y, circ.x);
			vec3 diffraction;
			diffraction.r = sin(angle * 3.0 + u_time * 0.150 * waver + dist * 10.0) * 0.5 + 0.5;
			diffraction.g = sin(angle * 3.0 + u_time * 0.450 * waver + dist * 10.0 + 2.0) * 0.5 + 0.5;
			diffraction.b = sin(angle * 3.0 + u_time * 0.650 * waver + dist * 10.0 + 4.0) * 0.5 + 0.5;

			// Glittering sparkles using Worley noise
			float sparkle = pow(fastWorley3d(v_pos.xyz * 10.0 + u_time * 2.0), 6.0);
			vec3 sparkle_color = vec3(1.0, 0.9, 0.6) * sparkle * 10.0;

			color = mix(vec3(1.0, 1.0, 1.0), iridescent_color, 0.7) * penumbra;
			color += core * 2.0;
			color = mix(color, diffraction * 2.0, ring * twinkle);
			color += sparkle_color * penumbra;

			alpha = v_p.color.a * shapeMask;
		} else if (v_style == STYLE_POOF) {
			float angle = v_p.phase;
			float s = sin(angle);
			float c = cos(angle);
			mat2  rot = mat2(c, -s, s, c);
			vec2  rotatedUV = rot * circ;

			float noise = fastWarpedFbm3d(vec3(rotatedUV / 5.0, v_lifetime / 0.1));
			float maskRadius = 0.5 * (v_p.color.a);
			shapeMask = 1.0;// - smoothstep(maskRadius - 0.1, maskRadius, length(circ));

			color = vec3(0.8, 0.8, 0.8) * (0.5 + 0.5 * noise);
			alpha = v_p.color.a;// * shapeMask;
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
