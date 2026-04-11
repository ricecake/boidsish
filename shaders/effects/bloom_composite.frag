#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
uniform float     intensity;
uniform float     minIntensity;
uniform float     maxIntensity;

// Auto-exposure SSBO
layout(std430, binding = 11) buffer AutoExposure {
	float adaptedLuminance;
	float targetLuminance;
	float minExposure;
	float maxExposure;
	int   useAutoExposure;
};

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

	// // Tone mapping to prevent oversaturation
	// result = vec3(1.0) - exp(-result * 1.0);

	// // Gamma correction
	// result = pow(result, vec3(1.0 / 2.2));

	FragColor = vec4(result, 1.0);
}
