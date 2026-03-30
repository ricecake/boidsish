#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D srcTexture;

// Dual Kawase blur downsample
// Optimized 5-tap filter using bilinear sampling to cover a 13-tap area
void main() {
	vec2 texelSize = 1.0 / vec2(textureSize(srcTexture, 0));

	// Sample center (covers 4 source texels)
	vec3 result = texture(srcTexture, TexCoords).rgb * 4.0;

	// Sample 4 corners at distance 1.0 (each covers 4 source texels)
	// This results in a high-quality blur with fewer texture fetches
	result += texture(srcTexture, TexCoords + vec2(-1.0, -1.0) * texelSize).rgb;
	result += texture(srcTexture, TexCoords + vec2(1.0, -1.0) * texelSize).rgb;
	result += texture(srcTexture, TexCoords + vec2(-1.0, 1.0) * texelSize).rgb;
	result += texture(srcTexture, TexCoords + vec2(1.0, 1.0) * texelSize).rgb;

	FragColor = vec4(result / 8.0, 1.0);
}
