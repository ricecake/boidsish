#version 330 core
out vec4 FragColor;

in vec3  vs_color;
in vec3  vs_normal;
in vec3  vs_frag_pos;
in float vs_progress;

#include "helpers/lighting_simple.glsl"

uniform bool  useIridescence;
uniform bool  useRocketTrail;
uniform bool  usePBR;         // Enable PBR lighting for trails
uniform float trailRoughness; // PBR roughness [0=mirror, 1=matte]
uniform float trailMetallic;  // PBR metallic [0=dielectric, 1=metal]

#include "helpers/noise.glsl"

void main() {
	vec3 norm = normalize(vs_normal);
	vec3 view_dir = normalize(viewPos - vs_frag_pos);

	if (useRocketTrail) {
		// --- Rocket Trail Effect with Emissive Glow ---
		float flame_threshold = 0.9;
		float flicker = snoise(vec2(time * 20.0, vs_frag_pos.x * 5.0)) * 0.5 + 0.5;

		if (vs_progress > flame_threshold) {
			// --- Flame (Emissive) ---
			float flame_progress = (vs_progress - flame_threshold) / (1.0 - flame_threshold);

			// Core of the flame (white hot, emissive)
			float core_intensity = pow(flame_progress, 8.0) * flicker;
			vec3  core_color = vec3(1.0, 1.0, 0.9);                   // White-hot
			vec3  flame_emission = core_color * core_intensity * 3.0; // HDR emissive

			// Outer flame glow (orange/yellow, also emissive)
			float outer_intensity = pow(flame_progress, 2.0) * (1.0 - core_intensity * 0.5);
			vec3  outer_color = vec3(1.0, 0.5, 0.1); // Orange
			flame_emission += outer_color * outer_intensity * 2.0;

			// Add subtle blue at the very base (hottest part)
			float blue_intensity = pow(flame_progress, 12.0) * 0.3;
			flame_emission += vec3(0.3, 0.5, 1.0) * blue_intensity;

			FragColor = vec4(flame_emission, 1.0);
		} else {
			// --- Smoke with subtle lighting ---
			float smoke_progress = vs_progress / flame_threshold;

			// Base smoke color
			vec3 smoke_color = mix(vec3(0.4, 0.4, 0.45), vs_color * 0.5, 0.2);

			// Apply simple lighting to smoke for depth
			vec3 lit_smoke = apply_lighting(vs_frag_pos, norm, smoke_color, 0.4, 0.1);

			// Noise for cloudy texture
			float noise = snoise(vec2(vs_progress * 5.0, time * 2.0)) * 0.5 + 0.5;

			// Alpha fades out at the tail
			float alpha = smoothstep(0.0, 0.2, smoke_progress) * (0.35 + noise * 0.25);

			// Subtle inner glow near the flame
			float glow = smoothstep(0.7, 0.9, smoke_progress) * 0.3;
			lit_smoke += vec3(1.0, 0.4, 0.1) * glow;

			FragColor = vec4(lit_smoke, alpha);
		}
	} else if (useIridescence) {
		// --- PBR Iridescent Trail ---
		// Low roughness for sharp, mirror-like reflections
		float roughness = usePBR ? trailRoughness : 0.15;

		// Apply PBR iridescent lighting
		vec3 iridescent_result = apply_lighting_pbr_iridescent(
			vs_frag_pos,
			norm,
			vs_color,
			roughness,
			1.0 // Full iridescence strength
		);

		// Add Fresnel rim for extra pop at grazing angles
		float fresnel = pow(1.0 - abs(dot(view_dir, norm)), 5.0);
		vec3  final_color = mix(iridescent_result, vec3(1.0), fresnel * 0.3);

		FragColor = vec4(final_color, 0.85); // Slightly more opaque for better visibility
	} else if (usePBR) {
		// --- Standard PBR Trail ---
		vec3 result = apply_lighting_pbr_simple(vs_frag_pos, norm, vs_color, trailRoughness, trailMetallic);
		FragColor = vec4(result, 1.0);
	} else {
		// --- Original Phong Lighting ---
		vec3 result = apply_lighting(vs_frag_pos, norm, vs_color, 0.2, 0.5);
		FragColor = vec4(result, 1.0);
	}
}
