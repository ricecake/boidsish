#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
uniform float     intensity;
uniform float     minIntensity;
uniform float     maxIntensity;

uniform bool toneMappingEnabled = false;
uniform int  toneMapMode = 2;

#include "helpers/tonemapping.glsl"

void main() {
	vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
	vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;

	float actualIntensity = intensity;
	if (useAutoExposure != 0) {
		actualIntensity *= (adaptedLuminance / max(targetLuminance, 0.0001));
		actualIntensity = clamp(actualIntensity, minIntensity, maxIntensity);
	}

	// Additive blending
	vec3 result = sceneColor + bloomColor * actualIntensity;

	if (toneMappingEnabled) {
		if (useAutoExposure != 0) {
			float exposure = targetLuminance / max(adaptedLuminance, 0.0001);
			exposure = clamp(exposure, minExposure, maxExposure);
			result *= exposure;
		}
		result = applyTonemapping(result, toneMapMode);
	}

	FragColor = vec4(result, 1.0);
}
