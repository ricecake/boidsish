#version 430 core

#extension GL_GOOGLE_include_directive : enable
#include "helpers/lighting.glsl"

in vec3 fFragPos;
in vec3 fNormal;
in vec2 fTexCoords;
in vec4 fColor;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;
layout(location = 2) out vec4 NormalRoughness;
layout(location = 3) out vec4 AlbedoMetallic;

uniform sampler2D u_texture;
uniform bool      u_useTexture;
uniform mat4      u_view;

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
	NormalRoughness = vec4(normalize(mat3(u_view) * norm) * 0.5 + 0.5, 0.5);
	AlbedoMetallic = vec4(baseColor, 0.0);
}
