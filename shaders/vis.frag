#version 420 core
out vec4 FragColor;

#include "helpers/lighting.glsl"
#include "visual_effects.frag"
#include "visual_effects.glsl"

in vec3 FragPos;
in vec3 Normal;
in vec3 vs_color;
in vec3 barycentric;
in vec2 TexCoords;
in vec4 InstanceColor;

uniform vec3  objectColor;
uniform float objectAlpha = 1.0;
uniform int   useVertexColor;
uniform bool  isColossal = false;
uniform bool  useInstanceColor = false;
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

uniform sampler2D texture_diffuse1;
uniform bool      use_texture;

void main() {
	float fade = 1.0;
	if (!isColossal) {
		float dist = length(FragPos.xz - viewPos.xz);
		float fade_start = 540.0 * worldScale;
		float fade_end = 550.0 * worldScale;
		fade = 1.0 - smoothstep(fade_start, fade_end, dist);

		if (fade < 0.2) {
			discard;
		}
	}

	vec3 final_color;
	if (useInstanceColor) {
		final_color = InstanceColor.rgb;
	} else if (useVertexColor == 1) {
		final_color = vs_color;
	} else {
		final_color = objectColor;
	}

	vec3 norm = normalize(Normal);

	float baseAlpha = objectAlpha;
	if (useInstanceColor) {
		baseAlpha = InstanceColor.a;
	}

	// Choose between PBR and legacy lighting
	vec4 lightResult;
	if (usePBR) {
		lightResult = apply_lighting_pbr(FragPos, norm, final_color * baseAlpha, roughness, metallic, ao);
	} else {
		lightResult = apply_lighting(FragPos, norm, final_color * baseAlpha, 1.0);
	}

	vec3  result = lightResult.rgb;
	float spec_lum = lightResult.a;

	if (use_texture) {
		result *= texture(texture_diffuse1, TexCoords).rgb;
	}

	result = applyArtisticEffects(result, FragPos, barycentric, time);

	if (isLine && lineStyle == 1) { // LASER style
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
	if (isLine && lineStyle == 1) {
		float distToCenter = abs(TexCoords.y - 0.5) * 2.0;
		float alpha = max(smoothstep(0.15, 0.08, distToCenter), exp(-distToCenter * 3.0) * 0.8);
		outColor = vec4(result, alpha * fade * objectAlpha);
	} else if (isColossal) {
		vec3  skyColor = vec3(0.2, 0.4, 0.8);
		float haze_start = 0.0;
		float haze_end = 150.0;
		float haze_factor = 1.0 - smoothstep(haze_start, haze_end, FragPos.y);
		vec3  final_haze_color = mix(result, skyColor, haze_factor * 0.5);
		outColor = vec4(final_haze_color, 1.0);
	} else {
		float final_alpha = clamp((baseAlpha + spec_lum) * fade, 0.0, 1.0);

		if (isTextEffect) {
			float alpha_factor = 1.0;
			if (textFadeMode == 0) { // Fade In
				alpha_factor = smoothstep(TexCoords.x, TexCoords.x + textFadeSoftness, textFadeProgress);
			} else if (textFadeMode == 1) { // Fade Out
				alpha_factor = 1.0 - smoothstep(TexCoords.x, TexCoords.x + textFadeSoftness, textFadeProgress);
			}
			final_alpha *= alpha_factor;
		}

		if (isArcadeText && arcadeRainbowEnabled) {
			vec3 rainbow = 0.5 +
				0.5 * cos(time * arcadeRainbowSpeed + TexCoords.x * arcadeRainbowFrequency + vec3(0, 2, 4));
			result *= rainbow;
		}

		outColor = vec4(result, final_alpha);
		outColor = mix(vec4(0.0, 0.7, 0.7, final_alpha) * length(outColor), outColor, step(1, fade));
	}

	FragColor = outColor;
}
