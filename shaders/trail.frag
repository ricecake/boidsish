#version 460 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

in vec3     vs_color;
in vec3     vs_normal;
in vec3     vs_frag_pos;
in float    vs_progress;
in vec4     CurPosition;
in vec4     PrevPosition;
flat in int vUniformIndex;

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;

#include "helpers/lighting.glsl"

uniform bool  useIridescence;
uniform bool  useRocketTrail;
uniform bool  usePBR;         // Enable PBR lighting for trails
uniform float trailRoughness; // PBR roughness [0=mirror, 1=matte]
uniform float trailMetallic;  // PBR metallic [0=dielectric, 1=metal]

#include "helpers/noise.glsl"

void main() {
	bool use_ssbo = uUseMDI && vUniformIndex >= 0;

	int   flags = use_ssbo ? uniforms_data[vUniformIndex].is_line : 0;
	bool  current_useIridescence = use_ssbo ? (flags & 1) != 0 : useIridescence;
	bool  current_useRocketTrail = use_ssbo ? (flags & 2) != 0 : useRocketTrail;
	bool  current_usePBR = use_ssbo ? (uniforms_data[vUniformIndex].use_pbr != 0) : usePBR;
	float current_trailRoughness = use_ssbo ? uniforms_data[vUniformIndex].roughness : trailRoughness;
	float current_trailMetallic = use_ssbo ? uniforms_data[vUniformIndex].metallic : trailMetallic;

	vec3 norm = normalize(vs_normal);
	vec3 view_dir = normalize(viewPos - vs_frag_pos);

	// Distance-based fade to prevent blocking the camera
	float dist_to_cam = length(viewPos - vs_frag_pos);
	float camera_fade = smoothstep(3.0, 10.0, dist_to_cam);

	if (current_useRocketTrail) {
		// --- Rocket Trail Effect with Emissive Glow ---
		float flame_threshold = 0.9;
		float flicker = snoise(vec2(time * 20.0, vs_frag_pos.x * 5.0)) * 0.5 + 0.5;

		if (vs_progress > flame_threshold) {
			// --- Flame (Emissive) ---
			float flame_progress = (vs_progress - flame_threshold) / (1.0 - flame_threshold);

			// Core of the flame (white hot, emissive)
			float core_intensity = pow(flame_progress, 8.0) * flicker;
			vec3  core_color = vec3(1.0, 1.0, 0.9);                   // White-hot
			vec3  flame_emission = core_color * core_intensity * 6.0; // HDR emissive (Increased)

			// Outer flame glow (orange/yellow, also emissive)
			float outer_intensity = pow(flame_progress, 2.0) * (1.0 - core_intensity * 0.5);
			vec3  outer_color = vec3(1.0, 0.4, 0.0);               // More intense Orange
			flame_emission += outer_color * outer_intensity * 5.0; // Increased

			// Add subtle blue at the very base (hottest part)
			float blue_intensity = pow(flame_progress, 12.0) * 0.3;
			flame_emission += vec3(0.3, 0.5, 1.0) * blue_intensity;

			FragColor = vec4(flame_emission, camera_fade);
		} else {
			// --- Smoke with subtle lighting ---
			float smoke_progress = vs_progress / flame_threshold;

			// Base smoke color
			vec3 smoke_color = mix(vec3(0.4, 0.4, 0.45), vs_color * 0.5, 0.2);

			// Apply simple lighting to smoke for depth (no shadows needed for trails)
			float dummyShadow;
			vec3 lit_smoke = apply_lighting_no_shadows(vs_frag_pos, norm, smoke_color, 0.1, dummyShadow).rgb;

			// Noise for cloudy texture
			float noise = snoise(vec2(vs_progress * 5.0, time * 2.0)) * 0.5 + 0.5;

			// Alpha fades out at the tail
			float alpha = smoothstep(0.0, 0.2, smoke_progress) * (0.35 + noise * 0.25);

			// Subtle inner glow near the flame
			float glow = smoothstep(0.7, 0.9, smoke_progress) * 0.3;
			lit_smoke += vec3(1.0, 0.4, 0.1) * glow;

			FragColor = vec4(lit_smoke, alpha * camera_fade);
		}
	} else if (current_useIridescence) {
		// --- PBR Iridescent Trail ---
		// Low roughness for sharp, mirror-like reflections
		float roughness = current_usePBR ? current_trailRoughness : 0.15;

		// Apply PBR iridescent lighting (no shadows for trail effects)
		float dummyShadowIrr;
		vec4 iridescent_result_vec = apply_lighting_pbr_iridescent_no_shadows(
			vs_frag_pos,
			norm,
			vs_color,
			roughness,
			1.0, // Full iridescence strength
			dummyShadowIrr
		);
		vec3 iridescent_result = iridescent_result_vec.rgb;

		// Add Fresnel rim for extra pop at grazing angles
		float fresnel = pow(1.0 - abs(dot(view_dir, norm)), 5.0);
		vec3  final_color = mix(iridescent_result, vec3(1.0), fresnel * 0.3);

		FragColor = vec4(final_color, 0.85 * camera_fade); // Slightly more opaque for better visibility
	} else if (current_usePBR) {
		// --- Standard PBR Trail (no shadows for trails) ---
		float dummyShadowPBR;
		vec3 result = apply_lighting_pbr_no_shadows(
						  vs_frag_pos,
						  norm,
						  vs_color,
						  current_trailRoughness,
						  current_trailMetallic,
						  1.0,
						  dummyShadowPBR
		)
						  .rgb;
		FragColor = vec4(result, camera_fade);
	} else {
		// --- Original Phong Lighting ---
		float dummyShadowPhong;
		vec3 result = apply_lighting_no_shadows(vs_frag_pos, norm, vs_color, 0.5, dummyShadowPhong).rgb;
		FragColor = vec4(result, camera_fade);
	}

	// Calculate screen-space velocity
	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = a - b;
}
