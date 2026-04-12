#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform int       toneMapMode = 2;

#include "helpers/tonemapping.glsl"

void main() {
	vec2 uv = TexCoords;
	vec3 tex = texture(sceneTexture, uv).rgb;

	if (useAutoExposure != 0) {
		float exposure = targetLuminance / max(adaptedLuminance, 0.0001);
		exposure = clamp(exposure, minExposure, maxExposure);
		tex *= exposure;
	}

	FragColor = vec4(applyTonemapping(tex, toneMapMode), 1.0f);
}
