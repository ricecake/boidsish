#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D srcTexture;

// Dual Kawase blur downsample
// Samples in a 3x3 pattern with wider spacing for better blur quality
void main() {
	vec2 texelSize = 1.0 / vec2(textureSize(srcTexture, 0));

	// Sample center
	vec3 result = texture(srcTexture, TexCoords).rgb * 4.0;

	// Sample 4 corners at distance 1
	result += texture(srcTexture, TexCoords + vec2(-1.0, -1.0) * texelSize).rgb;
	result += texture(srcTexture, TexCoords + vec2(1.0, -1.0) * texelSize).rgb;
	result += texture(srcTexture, TexCoords + vec2(-1.0, 1.0) * texelSize).rgb;
	result += texture(srcTexture, TexCoords + vec2(1.0, 1.0) * texelSize).rgb;

	// Sample 4 edges at distance 2 for wider blur
	result += texture(srcTexture, TexCoords + vec2(0.0, -2.0) * texelSize).rgb * 0.5;
	result += texture(srcTexture, TexCoords + vec2(0.0, 2.0) * texelSize).rgb * 0.5;
	result += texture(srcTexture, TexCoords + vec2(-2.0, 0.0) * texelSize).rgb * 0.5;
	result += texture(srcTexture, TexCoords + vec2(2.0, 0.0) * texelSize).rgb * 0.5;

	FragColor = vec4(result / 10.0, 1.0);
}
