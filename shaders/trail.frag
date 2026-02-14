#version 420 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec3 NormalColor;
layout(location = 2) out vec2 PbrColor;

in vec3  vColor;
in vec3  vNormal;
in vec3  vWorldPos;
in float vAlpha;
in float vThickness;

#include "helpers/lighting.glsl"

// Rocket trail effects
uniform bool  isRocket = false;

// PBR properties
uniform bool  usePBR = false;
uniform float roughness = 0.3;
uniform float metallic = 0.0;

void main() {
	vec3 norm = normalize(vNormal);

	if (isRocket) {
		// Animated flame effect for rocket trails
		float pulse = sin(time * 20.0 + vWorldPos.x * 2.0) * 0.2 + 0.8;
		vec3  flameColor = mix(vColor, vec3(1.0, 0.6, 0.2), 0.5);
		FragColor = vec4(flameColor * pulse, vAlpha);
		NormalColor = norm;
		PbrColor = vec2(0.5, 0.0);
		return;
	}

	vec4 litColor;
	if (usePBR) {
		litColor = apply_lighting_pbr_no_shadows(vWorldPos, norm, vColor, roughness, metallic, 1.0);
	} else {
		litColor = apply_lighting_no_shadows(vWorldPos, norm, vColor, 0.5);
	}

	FragColor = vec4(litColor.rgb, vAlpha);
	NormalColor = norm;
	PbrColor = vec2(usePBR ? roughness : 0.5, usePBR ? metallic : 0.0);
}
