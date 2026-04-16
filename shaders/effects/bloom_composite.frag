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

	// Add bloom to HDR scene color.
    // We removed the manual 'actualIntensity' scaling here because bloom should scale
    // with exposure naturally just like the rest of the scene.
	vec3 result = sceneColor + bloomColor * intensity;

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
