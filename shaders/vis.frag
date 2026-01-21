#version 330 core
out vec4 FragColor;

#include "helpers/lighting.glsl"
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

uniform sampler2D shadowMap;
uniform sampler2D texture_diffuse1;
uniform bool      use_texture;

float calculate_shadow(vec3 fragPos) {
    if (shadow_caster_index < 0) {
        return 0.0;
    }
    // Transform fragment position to light space
    vec4 fragPosLightSpace = lights[shadow_caster_index].lightSpaceMatrix * vec4(fragPos, 1.0);
    // Perspective division
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // Get closest depth value from light's perspective (using light's orthographic projection)
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // Add a bias to prevent shadow acne
    float bias = 0.005;
    // Check if fragment is in shadow
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    return shadow;
}

void main() {
	vec3 final_color;
	if (useVertexColor == 1) {
		final_color = vs_color;
	} else {
		final_color = objectColor;
	}

	vec3 norm = normalize(Normal);
	vec3 result = apply_lighting(FragPos, norm, final_color, 0.1, 1.0);
	float shadow = calculate_shadow(FragPos);
	result *= (1.0 - shadow * 0.5); // Simple shadow application

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
