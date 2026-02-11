#version 420 core

out vec4 FragColor;

#include "helpers/lighting.glsl"

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform float growthProgress;
uniform float fadeProgress;
uniform int   phase; // 0: GROWING, 1: FADING

void main() {
	vec3 N = normalize(Normal);
	vec3 V = normalize(viewPos - FragPos);

	vec3  baseColor;
	float alpha = 1.0;
	float emissive = 0.0;

	if (phase == 0) {
		// Phase 0: Growing - Emissive matte white
		baseColor = vec3(1.0);
		emissive = 5.0; // High intensity for bloom
	} else {
		// Phase 1: Fading - Metallic silver + Iridescence
		vec3 silver = vec3(0.75, 0.75, 0.8);

		// Transition from white to silver
		baseColor = mix(vec3(1.0), silver, fadeProgress);

		// Alpha fade out
		alpha = 1.0 - fadeProgress;

		// Emissive fade out
		emissive = mix(5.0, 0.0, fadeProgress);
	}

	vec4 litColor;
	if (phase == 0) {
		litColor = vec4(baseColor * (1.0 + emissive), alpha);
	} else {
		// Apply iridescence logic
		litColor = apply_lighting_pbr_iridescent_no_shadows(FragPos, N, baseColor, 0.05, 0.9);
		litColor.a *= alpha;

		// Add some remaining emissive glow
		litColor.rgb += baseColor * emissive;
	}

	FragColor = litColor;
}
