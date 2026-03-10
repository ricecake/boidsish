#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
uniform sampler2D horizontalFlare;
uniform sampler2D verticalFlare;

uniform float intensity;
uniform float flareIntensity;
uniform float horizontalIntensity;
uniform float verticalIntensity;

void main() {
	vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
	vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
	vec3 hFlare = texture(horizontalFlare, TexCoords).rgb;
	vec3 vFlare = texture(verticalFlare, TexCoords).rgb;

	// Additive blending
	vec3 result = sceneColor + bloomColor * intensity;

	// Add lens flares
	result += hFlare * flareIntensity * horizontalIntensity;
	result += vFlare * flareIntensity * verticalIntensity;

	// // Tone mapping to prevent oversaturation
	// result = vec3(1.0) - exp(-result * 1.0);

	// // Gamma correction
	// result = pow(result, vec3(1.0 / 2.2));

	FragColor = vec4(result, 1.0);
}
