#version 460 core
#extension GL_GOOGLE_include_directive : enable
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
flat in int  vUniformIndex;

#include "helpers/lighting.glsl"
#include "visual_effects.frag"
#include "visual_effects.glsl"

in vec3  FragPos;
in vec4  CurPosition;
in vec4  PrevPosition;
in vec3  Normal;
in vec3  vs_color;
in vec3  barycentric;
in vec2  TexCoords;
in vec4  InstanceColor;
in float WindDeflection;

uniform vec3  objectColor;
uniform float objectAlpha = 1.0;
uniform int   useVertexColor;
uniform bool  isColossal = false;
uniform bool  isLine = false;
uniform int   lineStyle = 0; // 0: SOLID, 1: LASER

uniform bool  isTextEffect = false;
uniform float textFadeProgress = 1.0;
uniform float textFadeSoftness = 0.1;
uniform int   textFadeMode = 0; // 0: Fade In, 1: Fade Out

// Arcade Text Effects
uniform bool  isArcadeText = false;
uniform bool  arcadeRainbowEnabled = false;
uniform float arcadeRainbowSpeed = 2.0;
uniform float arcadeRainbowFrequency = 5.0;

// PBR material properties
uniform bool  usePBR = false;
uniform float roughness = 0.5;
uniform float metallic = 0.0;
uniform float ao = 1.0;

uniform bool  dissolve_enabled = false;
uniform vec3  dissolve_plane_normal = vec3(0, 1, 0);
uniform float dissolve_plane_dist = 0.0;
uniform float dissolve_fade_thickness = 0.5;

uniform sampler2D texture_diffuse1;
uniform bool      use_texture;
uniform float     u_windRimHighlight;

void main() {
	bool  use_ssbo = uUseMDI && vUniformIndex >= 0;
	vec3  c_objectColor = use_ssbo ? uniforms_data[vUniformIndex].color.rgb : objectColor;
	float c_objectAlpha = use_ssbo ? uniforms_data[vUniformIndex].color.a : objectAlpha;
	bool  c_usePBR = use_ssbo ? (uniforms_data[vUniformIndex].use_pbr != 0) : usePBR;
	float c_roughness = use_ssbo ? uniforms_data[vUniformIndex].roughness : roughness;
	float c_metallic = use_ssbo ? uniforms_data[vUniformIndex].metallic : metallic;
	float c_ao = use_ssbo ? uniforms_data[vUniformIndex].ao : ao;
	bool  c_use_texture = use_ssbo ? (uniforms_data[vUniformIndex].use_texture != 0) : use_texture;
	bool  c_isLine = use_ssbo ? (uniforms_data[vUniformIndex].is_line != 0) : isLine;
	int   c_lineStyle = use_ssbo ? uniforms_data[vUniformIndex].line_style : lineStyle;
	bool  c_isTextEffect = use_ssbo ? (uniforms_data[vUniformIndex].is_text_effect != 0) : isTextEffect;
	float c_textFadeProgress = use_ssbo ? uniforms_data[vUniformIndex].text_fade_progress : textFadeProgress;
	float c_textFadeSoftness = use_ssbo ? uniforms_data[vUniformIndex].text_fade_softness : textFadeSoftness;
	int   c_textFadeMode = use_ssbo ? uniforms_data[vUniformIndex].text_fade_mode : textFadeMode;
	bool  c_isArcadeText = use_ssbo ? (uniforms_data[vUniformIndex].is_arcade_text != 0) : isArcadeText;
	bool  c_arcadeRainbowEnabled = use_ssbo ? (uniforms_data[vUniformIndex].arcade_rainbow_enabled != 0)
                                           : arcadeRainbowEnabled;
	float c_arcadeRainbowSpeed = use_ssbo ? uniforms_data[vUniformIndex].arcade_rainbow_speed : arcadeRainbowSpeed;
	float c_arcadeRainbowFrequency = use_ssbo ? uniforms_data[vUniformIndex].arcade_rainbow_frequency
											  : arcadeRainbowFrequency;
	bool  c_isColossal = use_ssbo ? (uniforms_data[vUniformIndex].is_colossal != 0) : isColossal;
	bool  c_useVertexColor = use_ssbo ? (uniforms_data[vUniformIndex].use_vertex_color != 0) : (useVertexColor != 0);

	bool  c_dissolve_enabled = use_ssbo ? (uniforms_data[vUniformIndex].dissolve_enabled != 0)
                                       : dissolve_enabled;
	vec3  c_dissolve_normal = use_ssbo ? uniforms_data[vUniformIndex].dissolve_plane_normal : dissolve_plane_normal;
	float c_dissolve_dist = use_ssbo ? uniforms_data[vUniformIndex].dissolve_plane_dist : dissolve_plane_dist;
	float c_dissolve_fade_thickness = use_ssbo ? uniforms_data[vUniformIndex].dissolve_fade_thickness
											   : dissolve_fade_thickness;

	float fade = 1.0;
	if (c_dissolve_enabled) {
		float dist_to_plane = dot(FragPos, c_dissolve_normal) - c_dissolve_dist;
		// Trail behind the slice: alpha 1 at the slice, fading to 0 over thickness in the swept direction
		float dissolve_fade = clamp(1.0 - dist_to_plane / max(0.001, c_dissolve_fade_thickness), 0.0, 1.0);
		fade *= dissolve_fade;

		if (fade <= 0.001) {
			discard;
		}
	}

	if (!c_isColossal) {
		float dist = length(FragPos.xz - viewPos.xz);
		float fade_start = 540.0 * worldScale;
		float fade_end = 550.0 * worldScale;
		fade = 1.0 - smoothstep(fade_start, fade_end, dist);

		if (fade < 0.2) {
			discard;
		}
	}

	vec3 final_color;
	if (c_useVertexColor) {
		final_color = vs_color;
	} else {
		final_color = c_objectColor;
	}

	vec3 norm = normalize(Normal);

	float baseAlpha = c_objectAlpha;

	// Choose between PBR and legacy lighting
	vec4 lightResult;
	if (c_usePBR) {
		lightResult = apply_lighting_pbr(FragPos, norm, final_color * baseAlpha, c_roughness, c_metallic, c_ao);
	} else {
		lightResult = apply_lighting(FragPos, norm, final_color * baseAlpha, 1.0);
	}

	vec3  result = lightResult.rgb;
	float spec_lum = lightResult.a;

	// Apply wind-driven rim highlight
	float rim = pow(1.0 - max(dot(norm, normalize(viewPos - FragPos)), 0.0), 3.0);
	result += rim * WindDeflection * u_windRimHighlight * vec3(1.0);

	if (c_use_texture) {
		result *= texture(texture_diffuse1, TexCoords).rgb;
	}

	result = applyArtisticEffects(result, FragPos, barycentric, time);

	if (c_isLine && c_lineStyle == 1) { // LASER style
		// Use Y axis for radial glow as defined in Line::InitLineMesh
		float distToCenter = abs(TexCoords.y - 0.5) * 2.0;

		// Solid core
		float core = smoothstep(0.15, 0.08, distToCenter);

		// Outer glow
		float glow = exp(-distToCenter * 3.0) * 0.8;

		// Inner glow for extra brightness
		float innerGlow = exp(-distToCenter * 10.0) * 0.5;

		vec3 coreColor = vec3(1.0, 1.0, 1.0); // Core is white
		vec3 glowColor = final_color;         // Glow is the object color

		vec3 laserColor = mix(glowColor, coreColor, core);
		laserColor += glowColor * glow;
		laserColor += coreColor * innerGlow;

		result = laserColor;
	}

	vec4 outColor;

	// Check for laser style first to ensure transparency/glow is handled correctly
	if (c_isLine && c_lineStyle == 1) {
		float distToCenter = abs(TexCoords.y - 0.5) * 2.0;
		float alpha = max(smoothstep(0.15, 0.08, distToCenter), exp(-distToCenter * 3.0) * 0.8);
		outColor = vec4(result, alpha * fade * c_objectAlpha);
	} else if (c_isColossal) {
		// Restore colossal haze style
		vec3  skyColor = vec3(0.2, 0.4, 0.8);
		float haze_start = 0.0;
		float haze_end = 150.0;
		float haze_factor = 1.0 - smoothstep(haze_start, haze_end, FragPos.y);
		vec3  final_haze_color = mix(result, skyColor, haze_factor * 0.5);
		outColor = vec4(final_haze_color, 1.0);
	} else {
		float final_alpha = clamp((baseAlpha + spec_lum) * fade, 0.0, 1.0);

		if (c_isTextEffect) {
			float alpha_factor = 1.0;
			if (c_textFadeMode == 0) { // Fade In
				alpha_factor = smoothstep(TexCoords.x, TexCoords.x + c_textFadeSoftness, c_textFadeProgress);
			} else if (c_textFadeMode == 1) { // Fade Out
				alpha_factor = 1.0 - smoothstep(TexCoords.x, TexCoords.x + c_textFadeSoftness, c_textFadeProgress);
			}
			final_alpha *= alpha_factor;
		}

		if (c_isArcadeText && c_arcadeRainbowEnabled) {
			vec3 rainbow = 0.5 +
				0.5 * cos(time * c_arcadeRainbowSpeed + TexCoords.x * c_arcadeRainbowFrequency + vec3(0, 2, 4));
			result *= rainbow;
		}

		outColor = vec4(result, final_alpha);
		// Restore deliberate cyan style for distant objects
		outColor = mix(vec4(0.0, 0.7, 0.7, final_alpha) * length(outColor), outColor, step(1.0, fade));
	}

	// if (nightFactor > 0) {
	// outColor += nightFactor * (sin(length(FragPos.xz - viewPos.xz * 0.1) + time) * 0.5 + 0.5) * vec4(0, 7, 0, 1);
	// }

	FragColor = outColor;

	// Calculate screen-space velocity
	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = a - b;
}
