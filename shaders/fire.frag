#version 430 core

#include "lighting.glsl"

in float    v_lifetime;
in vec4     view_pos;
in vec4     v_pos;
in vec3     v_epicenter;
flat in int v_style;
flat in int v_emitter_index;
flat in int v_emitter_id;
out vec4    FragColor;

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
	// Multipliers adjusted for high-emissivity stunt look
	return col * vec3(8.0, 2.5, 1.5);
}

float turbulence(vec2 p) {
	float sum = 0.0;
	float amp = 0.5;
	for (int i = 0; i < 4; i++) {
		sum += abs(snoise(p)) * amp;
		p *= 2.0;
		amp *= 0.5;
	}
	return sum;
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
	if (v_style == 0 || v_style == 3 || v_style == 4 || v_style == 5 || v_style == 6 || v_style == 7 || v_style == 8 ||
	    v_style == 9 || v_style == 28) {
		if (v_style == 3) {                       // Sparks
			vec3 hot_color = vec3(1.0, 1.0, 1.0); // White
			vec3 mid_color = vec3(1.0, 0.8, 0.3); // Bright Yellow/Orange
			color = mix(mid_color, hot_color, smoothstep(0.0, 0.5, v_lifetime));

			// Popping/flickering effect
			float pop = sin(v_lifetime * 600.0);
			if (pop > 0.0) {
				color *= 3.0;
			} else {
				color *= 0.3;
			}
			alpha = smoothstep(0.0, 0.1, v_lifetime);
		} else if (v_style == 4) { // Glitter
			// Glitter uses a colorful rainbow shift
			// We use u_time and v_lifetime to create motion in the color space
			float hue = u_time * 2.0 + v_lifetime * 1.5 + float(gl_PrimitiveID) * 0.1;
			color = 0.6 + 0.4 * cos(hue + vec3(0, 2, 4));

			// Add a bit of "twinkle" based on time and position
			float twinkle = sin(u_time * 15.0 + v_lifetime * 5.0 + gl_PointCoord.x * 10.0) * 0.5 + 0.5;
			color *= 0.6 + 0.4 * twinkle;

			// Add a "sparkle" highlight
			float sparkle = pow(twinkle, 10.0) * 2.0;
			color += vec3(sparkle);
			alpha = clamp(v_lifetime, 0.0, 1.0);
		} else if (v_style == 5) { // Ambient
			int  sub_style = v_emitter_id;
			vec3 biome_albedo = (v_emitter_index >= 0 && v_emitter_index < 8) ? u_biomeAlbedos[v_emitter_index]
																			  : vec3(0.5);

			if (sub_style == 0) { // Leaf
				vec3 leaf_green = vec3(0.2, 0.4, 0.1);
				vec3 leaf_brown = vec3(0.4, 0.3, 0.15);
				color = mix(leaf_green, leaf_brown, sin(v_pos.x * 0.1 + v_pos.z * 0.1) * 0.5 + 0.5);
				color = mix(color, biome_albedo, 0.3);

				float flutter = sin(u_time * 5.0 + v_pos.x + v_pos.y) * 0.3 + 0.7;
				color *= flutter;
				color += vec3(0.1) * pow(flutter, 10.0);
				alpha = smoothstep(0.0, 0.5, v_lifetime) * 0.9;
			} else if (sub_style == 1) { // Flower Petal
				// Vibrant petal colors based on biome but shifted
				vec3 petal_base = mix(biome_albedo, vec3(1.0, 0.5, 0.8), 0.6); // Lean towards pink/magenta
				if (v_emitter_index == 4)
					petal_base = vec3(1.0, 0.9, 0.2); // Alpine Meadow has yellow flowers

				float color_var = sin(float(gl_PrimitiveID) * 0.5) * 0.5 + 0.5;
				color = mix(petal_base, vec3(1.0, 1.0, 1.0), color_var * 0.4);

				float flutter = sin(u_time * 8.0 + v_pos.x * 2.0) * 0.4 + 0.6;
				color *= flutter;
				alpha = smoothstep(0.0, 0.5, v_lifetime) * 0.95;
			} else if (sub_style == 2) { // Bubble
				// Use local spherical normal for better visuals
				vec3 n;
				n.xy = circ * 2.0;
				float magSq = dot(n.xy, n.xy);
				n.z = sqrt(max(0.0, 1.0 - magSq));

				float fresnel = pow(max(0.0, 1.0 - n.z), 3.0);
				float swirl = sin(v_lifetime * 2.0 + gl_PointCoord.y * 5.0) * 0.5 + 0.5;
				vec3  iridescent_color = vec3(
                    sin(swirl * 5.0) * 0.5 + 0.5,
                    sin(swirl * 5.0 + 2.0) * 0.5 + 0.5,
                    sin(swirl * 5.0 + 4.0) * 0.5 + 0.5
                );
				// Better specular highlight
				vec3  l = normalize(vec3(0.5, 0.5, 1.0)); // Fake light dir in billboard space
				vec3  h = normalize(l + vec3(0, 0, 1));   // Halfway between light and view (0,0,1)
				float spec = pow(max(dot(n, h), 0.0), 64.0);

				color = mix(iridescent_color, vec3(1.0), fresnel * 0.5 + 0.2) + spec;
				alpha = 0.6 * smoothstep(0.0, 0.5, v_lifetime);
			} else if (sub_style == 3) { // Snowflake
				color = vec3(0.9, 0.95, 1.0) * (1.2 + 0.3 * sin(u_time * 2.0 + v_pos.x));
				alpha = 0.8 * smoothstep(0.0, 0.5, v_lifetime);
			} else if (sub_style == 4) {                      // Firefly
				vec3 firefly_base = vec3(0.7, 0.9, 0.1);      // Yellow-Green
				color = mix(firefly_base, biome_albedo, 0.4); // Biome biased
				float twinkle = sin(u_time * 6.0 + float(gl_PrimitiveID)) * 0.5 + 0.5;
				color *= (2.0 + twinkle * 8.0);
				alpha = (0.4 + twinkle * 0.6) * smoothstep(0.0, 0.5, v_lifetime);
			}
		} else if (v_style == 6) { // Bubbles
			// Use local spherical normal for better visuals
			vec3 n;
			n.xy = circ * 2.0;
			float magSq = dot(n.xy, n.xy);
			n.z = sqrt(max(0.0, 1.0 - magSq));

			float fresnel = pow(max(0.0, 1.0 - n.z), 3.0);
			float swirl = sin(v_lifetime * 2.0 + gl_PointCoord.y * 5.0) * 0.5 + 0.5;
			vec3  iridescent_color = vec3(
                sin(swirl * 5.0) * 0.5 + 0.5,
                sin(swirl * 5.0 + 2.0) * 0.5 + 0.5,
                sin(swirl * 5.0 + 4.0) * 0.5 + 0.5
            );
			vec3  l = normalize(vec3(0.5, 0.5, 1.0)); // Fake light dir in billboard space
			vec3  h = normalize(l + vec3(0, 0, 1));   // Halfway between light and view (0,0,1)
			float spec = pow(max(dot(n, h), 0.0), 64.0);

			color = mix(iridescent_color, vec3(1.0), fresnel * 0.5 + 0.2) + spec;
			alpha = 0.6 * smoothstep(0.0, 0.5, v_lifetime);
		} else if (v_style == 7) {                    // Fireflies
			vec3  firefly_base = vec3(0.7, 0.9, 0.1); // Yellow-Green
			float twinkle = sin(u_time * 6.0 + float(gl_PrimitiveID)) * 0.5 + 0.5;
			color = firefly_base * (2.0 + twinkle * 8.0);
			alpha = (0.4 + twinkle * 0.6) * smoothstep(0.0, 0.5, v_lifetime);
		} else if (v_style == 28) {
			// --- Iridescence Effect ---
			// Fresnel term for the base reflectivity
			float fresnel = pow(max(0.0, 1.0 - distSq * 4.0), 5.0);

			// Use view angle to create a color shift
			float angle_factor = clamp(1.0 - distSq * 4.0, 0.0, 1.0);
			angle_factor = pow(angle_factor, 2.0);

			// Use time and fragment position to create a swirling effect
			float swirl = sin(v_lifetime * 0.5 + gl_PointCoord.y * 2.0) * 0.5 + 0.5;

			// Combine for final color using a rainbow palette shifted by the swirl and angle
			vec3 iridescent_color = vec3(
				sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5,
				sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5,
				sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5
			);

			// Add a strong specular highlight
			float v_len = length(view_pos.xyz);
			vec3  view_dir = v_len > 0.001 ? normalize(-view_pos.xyz) : vec3(0, 0, 1);
			vec3  r = reflect(-view_dir, vec3(0, 1, 0)); // Simplified view-space reflection
			float spec = pow(max(dot(view_dir, r), 0.0), 64.0);
			vec3  specular = 1.5 * spec * vec3(1.0); // white highlight

			color = mix(iridescent_color, vec3(1.0), fresnel) + specular;
			alpha = 0.75;
		} else if (v_style == 8) {        // Debug
			float hue = v_lifetime * 0.5; // Shift through spectrum
			color = 0.6 + 0.4 * cos(hue * 6.28 + vec3(0, 2, 4));
			color *= 3.0; // Bright for bloom
			alpha = 1.0;
		} else if (v_style == 9) { // Cinder
			// Irregular shape via noise
			float n = snoise(vec3(gl_PointCoord * 6.0, float(gl_PrimitiveID)));
			shapeMask = smoothstep(0.2 + n * 0.15, 0.05, distSq);

			// Color: dark grey with orange highlights
			float cNoise = snoise(v_pos.xyz * 15.0 + u_time * 0.2);
			vec3  darkGrey = vec3(0.1);
			vec3  sootyGrey = vec3(0.25);
			color = mix(darkGrey, sootyGrey, cNoise * 0.5 + 0.5);

			float highlights = smoothstep(0.4, 0.9, snoise(v_pos.xyz * 40.0 + u_time));
			vec3  orange = vec3(2.5, 0.8, 0.2);
			color = mix(color, orange, highlights);

			alpha = smoothstep(0.0, 0.5, v_lifetime);
		} else if (v_style == 0) { // Exhaust (Smoke)
			color = vec3(0.1);     // Dark smoke
			alpha = v_lifetime * 0.4;
		}

		// Apply circular mask and premultiplied alpha to all simple styles
		alpha *= shapeMask;
		color *= alpha;
	} else {
		float maxLife = 1.0;
		if (v_style == 1) { // Explosion
			maxLife = kExplosionLifetime;
		} else if (v_style == 2) { // Default Fire
			maxLife = kFireLifetime;
		}

		float distFromEpicenter = length(v_pos.xyz - v_epicenter);
		float normalizedLife = clamp(v_lifetime / maxLife, 0.0, 1.0);

		// Broad roiling motion (low frequency)
		// Scale down for broader features, especially for explosions
		float roilScale = (v_style == 1) ? 0.015 : 0.03;
		vec3  roilCoords = v_pos.xyz * roilScale - vec3(0.0, u_time * 0.1, 0.0);
		float roil = fastFbm3d(roilCoords) * 0.5 + 0.5;

		// Worley "knoblyness" and broad structures
		// Scale by distance to increase structural variation as it expands
		float worleyScale = (v_style == 1) ? 0.05 : 0.1;
		float expansionFactor = 1.0 + distFromEpicenter * 0.03;
		vec3  worleyCoords = v_pos.xyz * worleyScale * expansionFactor + vec3(u_time * 0.05);
		float knobly = fastWorley3d(worleyCoords);

		// Combine structural noise
		float noiseDetail = mix(roil, knobly, abs(fastSimplex3d(vec3(0, u_time, 0))));

		// Reduced high-frequency gl_PointCoord influence
		float highFreq = fastSimplex3d(vec3(gl_PointCoord * 0.4, u_time * 0.05)) * 0.5 + 0.5;
		noiseDetail = mix(noiseDetail, noiseDetail * highFreq, 0.35);

		// Temperature map shaping: Cooler at particle edges
		// This creates a more volumetric, spherical look
		float radial = 1.0 - (distSq * 4.0); // 1.0 at center, 0.0 at radius 0.5
		float edgeCooling = pow(max(0.0, radial), 0.7);

		// Heat influenced by expansion for explosions
		float epicenterCooling = 1.0;
		if (v_style == 1) {
			epicenterCooling = smoothstep(80.0, 0.0, distFromEpicenter);
		}

		float heat = normalizedLife * pow(noiseDetail, 1.4) * edgeCooling * epicenterCooling;
		// turbulence(v_pos.xz * 0.4 + u_time * 0.1) * turbulence(gl_PointCoord + u_time * 0.3);
		vec3 baseColor = blackbody_hdr(heat);

		// Sharper alpha thresholds for "jagged" defined edges
		alpha = shapeMask * smoothstep(0.01, 0.12, heat) * turbulence(gl_PointCoord);
		color = baseColor * alpha * 12.0 * (1.0 + normalizedLife);
	}

	FragColor = vec4(color, alpha);
}