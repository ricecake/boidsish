#version 420 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec3 NormalColor;
layout(location = 2) out vec2 PbrColor;

in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;

#include "helpers/lighting.glsl"

uniform vec3  color;
uniform int   style; // 0: Gold, 1: Silver, 2: Black, 3: Blue, 4: Neon Green, 5: Rainbow, 6: Invisible

void main() {
	if (style == 6) {
		discard;
	}

	vec3  baseColor = color;
	float roughness = 0.2;
	float metallic = 0.8;

	if (style == 5) { // Rainbow
		baseColor = 0.5 + 0.5 * cos(time + TexCoords.xyx + vec3(0, 2, 4));
	}

	vec3 norm = normalize(Normal);
	vec4 litColor = apply_lighting_pbr_no_shadows(WorldPos, norm, baseColor, roughness, metallic, 1.0);

	// Add ring-like glow
	float dist = length(TexCoords - 0.5);
	float ring = smoothstep(0.45, 0.48, dist) * smoothstep(0.52, 0.49, dist);
	vec3  finalColor = litColor.rgb + baseColor * ring * 2.0;

	FragColor = vec4(finalColor, 0.8);
	NormalColor = norm;
	PbrColor = vec2(roughness, metallic);
}
