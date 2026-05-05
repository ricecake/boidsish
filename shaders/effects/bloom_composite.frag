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

uniform float uchimuraP;
uniform float uchimuraA;
uniform float uchimuraM;
uniform float uchimuraL;
uniform float uchimuraC;
uniform float uchimuraB;

#include "helpers/tonemapping.glsl"
#include "types/autoexposure.glsl"

void main() {
	vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
	vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;

	// Add bloom to HDR scene color.
    // We removed the manual 'actualIntensity' scaling here because bloom should scale
    // with exposure naturally just like the rest of the scene.
	vec3 result = sceneColor + bloomColor * intensity;

	if (useAutoExposure != 0) {
		float exposure = targetLuminance / max(adaptedLuminance, 0.0001);
		exposure = clamp(exposure, minExposure, maxExposure);
		result *= exposure;
	}

	if (toneMappingEnabled) {
		if (toneMapMode == 5) {
			result = uchimura(result, uchimuraP, uchimuraA, uchimuraM, uchimuraL, uchimuraC, uchimuraB);
		} else {
			result = applyTonemapping(result, toneMapMode);
		}
	}

	FragColor = vec4(result, 1.0);
}
