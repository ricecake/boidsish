#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
uniform float     intensity;

void main() {
	vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
	vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;

	// Additive blending
	vec3 result = sceneColor + bloomColor * intensity;

	// // Tone mapping to prevent oversaturation
	// result = vec3(1.0) - exp(-result * 1.0);

	// // Gamma correction
	// result = pow(result, vec3(1.0 / 2.2));

	FragColor = vec4(result, 1.0);
}
