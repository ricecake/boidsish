#version 330 core
out vec4 FragColor;

#include "lighting.glsl"
#include "visual_effects.frag"
#include "visual_effects.glsl"

in vec3 FragPos;
in vec3 Normal;
in vec3 vs_color;
in vec3 barycentric;
in vec2 TexCoords;

uniform vec3 objectColor;
uniform int  useVertexColor;
uniform bool isColossal = true;

uniform sampler2D texture_diffuse1;
uniform bool      use_texture;

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

	result = applyArtisticEffects(result, FragPos, barycentric, time);

	float dist = length(FragPos.xz - viewPos.xz);
	float fade_start = 540.0;
	float fade_end = 550.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

	vec4 outColor;

	if (isColossal) {
		vec3  skyColor = vec3(0.2, 0.4, 0.8);
		float haze_start = 0.0;
		float haze_end = 75.0;
		float haze_factor = 1.0 - smoothstep(haze_start, haze_end, FragPos.y);
		vec3  final_haze_color = mix(result, skyColor, haze_factor * 2);
		outColor = vec4(final_haze_color, mix(0, 1, 1 - (haze_factor)));
	} else {
		outColor = vec4(result, mix(0, fade, step(0.01, FragPos.y)));
		outColor = mix(
			vec4(0.0, 0.7, 0.7, mix(0, fade, step(0.01, FragPos.y))) * length(outColor),
			outColor,
			step(1, fade)
		);
	}

	FragColor = outColor;
}
