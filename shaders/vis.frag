#version 330 core
out vec4 FragColor;

#include "lighting.glsl"
#include "visual_effects.glsl"
#include "visual_effects.frag"

in vec3 FragPos;
in vec3 Normal;
in vec3 vs_color;
in vec3 barycentric;
in vec2 TexCoords;

uniform vec3 objectColor;
uniform int  useVertexColor;
uniform bool isColossal = true;

uniform sampler2D texture_diffuse1;
uniform bool use_texture;

void main() {
	// Ambient
	float ambientStrength = 0.1;
	vec3  ambient = ambientStrength * lightColor;

	// Diffuse
	vec3  norm = normalize(Normal);
	vec3  lightDir = normalize(lightPos - FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3  diffuse = diff * lightColor;

	// Specular
	float specularStrength = 1.0;
	vec3  viewDir = normalize(viewPos - FragPos);
	vec3  reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
	vec3  specular = specularStrength * spec * lightColor;

	// Rim
	float rimPower = 2.0;
	float rim = 1.0 - max(dot(viewDir, norm), 0.0);
	rim = pow(rim, rimPower);
	vec3 rimColor = rim * lightColor;

	vec3 final_color;
	if (useVertexColor == 1) {
		final_color = vs_color;
	} else {
		final_color = objectColor;
	}

	vec3 result = (ambient + diffuse) * final_color + specular;

	if (use_texture) {
		result *= texture(texture_diffuse1, TexCoords).rgb;
	}

	if (color_shift_enabled == 1) {
		float shift_magnitude = 0.2;
		float shift_speed = 5.0;
		vec3  pos_based_shift;
		pos_based_shift.r = sin(FragPos.x * shift_speed) * shift_magnitude;
		pos_based_shift.g = sin(FragPos.y * shift_speed) * shift_magnitude;
		pos_based_shift.b = sin(FragPos.z * shift_speed) * shift_magnitude;
		result += pos_based_shift;

		int posterize_levels = 5;
		result.r = floor(result.r * posterize_levels) / posterize_levels;
		result.g = floor(result.g * posterize_levels) / posterize_levels;
		result.b = floor(result.b * posterize_levels) / posterize_levels;
	}

	result = applyArtisticEffects(result, FragPos, barycentric, time);

	float dist = length(FragPos.xz - viewPos.xz);
	float fade_start = 540.0;
	float fade_end = 550.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

	vec4 outColor;

	if (isColossal) {
		// --- Colossal Object Atmospheric Haze ---
		// A brighter, more visible haze color
		vec3 skyColor = vec3(0.5, 0.6, 0.7);
		// Fade the object in as it rises above the horizon (Y=0)
		float haze_start = 0.0;
		// Make the fade happen more quickly so the object is visible
		float haze_end = 75.0;
		// We use 1.0 - smoothstep to fade *in* (reduce haze) as Y increases
		float haze_factor = 1.0 - smoothstep(haze_start, haze_end, FragPos.y);

		vec3 final_haze_color = mix(result, skyColor, haze_factor);
		outColor = vec4(final_haze_color, 1.0);
	} else {
		// --- Standard Object Fading ---
		outColor = vec4(result, mix(0, fade, step(0.01, FragPos.y)));
		outColor = mix(
			vec4(0.0, 0.7, 0.7, mix(0, fade, step(0.01, FragPos.y))) * length(outColor),
			outColor,
			step(1, fade)
		);
	}

	FragColor = outColor;
}
