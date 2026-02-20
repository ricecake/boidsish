#version 420 core

#include "helpers/lighting.glsl"

in vec3 fFragPos;
in vec3 fNormal;
in vec2 fTexCoords;
in vec4 fColor;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;
layout(location = 2) out vec3 WorldNormal;
layout(location = 3) out vec4 MaterialData;

uniform sampler2D u_texture;
uniform bool      u_useTexture;

void main() {
	if (fColor.a <= 0.0)
		discard;

	vec3 baseColor = fColor.rgb;
	if (u_useTexture) {
		baseColor *= texture(u_texture, fTexCoords).rgb;
	}

	vec3 norm = normalize(fNormal);

	// Use apply_lighting_no_shadows for better performance on many fragments
	// but still getting all scene lights and ambient light.
	vec4 lightResult = apply_lighting_no_shadows(fFragPos, norm, baseColor, 0.5);

	FragColor = vec4(lightResult.rgb, fColor.a);
	Velocity = vec2(0.0);
	WorldNormal = norm;
	MaterialData = vec4(0.5, 0.0, 1.0, 0.0);
}
