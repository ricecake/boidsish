#version 430 core

in float    v_lifetime;
in vec4     view_pos;
in vec4     v_pos;
in vec3     v_epicenter;
flat in int v_style;
out vec4    FragColor;

uniform float u_time;
#include "helpers/noise.glsl"
#include "helpers/fast_noise.glsl"

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

// Your existing warped turbulence logic
float turbulence(vec2 p) {
    float sum = 0.0;
    float amp = 0.5;
    for(int i = 0; i < 4; i++) {
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


	vec3 color;
	float alpha;
	if (v_style == 0 || v_style == 3 || v_style == 4 || v_style == 28) {
		if (v_style == 3) {                // Sparks
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
		}
		else if (v_style == 28) {
			// --- Iridescence Effect ---
			// Fresnel term for the base reflectivity
			float fresnel = pow(1.0 - distSq, 5.0);

			// Use view angle to create a color shift
			float angle_factor = 1.0 - distSq;
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
			vec3  reflect_dir = reflect(-view_pos + vec4(0, 20, 0, 0), vec4(distSq)).xyz;
			float spec = pow(max(dot(view_pos.xyz, reflect_dir), 0.0), 128.0);
			vec3  specular = 1.5 * spec * vec3(1.0); // white highlight

			color = mix(iridescent_color, vec3(1.0), fresnel) + specular;
		}

		if (v_style == 0) {
			alpha = v_lifetime * 0.4;
		} else if (v_style == 3) {
			alpha = smoothstep(0.0, 0.1, v_lifetime);
		} else if (v_style == 4) { // Glitter
			alpha = clamp(v_lifetime, 0.0, 1.0);
		} else if (v_style == 28) {
			alpha = 0.75;
		}
	}
	else {
		float maxLife = 1.0;
		if (v_style == 0) {        // Rocket Trail
			maxLife = kExhaustLifetime;
		} else if (v_style == 1) { // Explosion
			maxLife = kExplosionLifetime;
		} else if (v_style == 2) { // Default Fire
			maxLife = kFireLifetime;
		}

		float distFromEpicenter = length(v_pos.xyz - v_epicenter);

		// Broad roiling motion (low frequency)
		vec3 roilCoords = v_pos.xyz * 0.04 - vec3(0.0, u_time * 0.15, 0.0);
		float roil = fastFbm3d(roilCoords) * 0.5 + 0.5;

		// Worley "knoblyness" and broad structures
		// Scale by distance to increase detail as it expands
		float expansionFactor = 1.0 + distFromEpicenter * 0.05;
		vec3 worleyCoords = v_pos.xyz * 0.12 * expansionFactor + vec3(u_time * 0.08);
		float knobly = fastWorley3d(worleyCoords);

		// Combine structural noise, reducing high-frequency gl_PointCoord influence
		float noiseDetail = mix(roil, knobly, 0.5);

		// Add subtle high-frequency texture that moves with the roil
		float highFreq = fastSimplex3d(vec3(gl_PointCoord * 1.0, u_time * 0.2)) * 0.5 + 0.5;
		noiseDetail = mix(noiseDetail, noiseDetail * highFreq, 0.3);

		// Temperature map shaping: Cooler at particle edges
		// distSq is from gl_PointCoord: 0 at center, 0.25 at edge
		float edgeCooling = smoothstep(0.25, 0.02, distSq);

		// Heat also influenced by distance from epicenter for explosions
		float epicenterCooling = 1.0;
		if (v_style == 1) {
			epicenterCooling = smoothstep(60.0, 5.0, distFromEpicenter); // Cool down as it expands far
		}

		float heat = clamp(v_lifetime / maxLife, 0.0, 1.0) * pow(noiseDetail, 1.3) * edgeCooling * epicenterCooling;
		vec3  baseColor = blackbody_hdr(heat);

		alpha = shapeMask * smoothstep(0.02, 0.2, heat);
		color = baseColor * alpha * 10.0 * (1.0 + clamp(v_lifetime / maxLife, 0.0, 1.0));
	}

	FragColor = vec4(color, alpha);
}