#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D srcTexture;
uniform vec2      srcResolution;
uniform float     filterRadius; // Default 1.0
uniform float     srcLod = 0.0;

// Dual Kawase blur upsample with tent filter
// Provides smooth, high-quality upsampling with natural blur
void main() {
	vec2  texelSize = 1.0 / srcResolution;
	float radius = filterRadius;

	// 9-tap tent filter for smooth upsampling
	vec3 result = vec3(0.0);

	// Center sample (weight 4)
	result += textureLod(srcTexture, TexCoords, srcLod).rgb * 4.0;

	// Diagonal samples at half-pixel offset (weight 1 each)
	result += textureLod(srcTexture, TexCoords + vec2(-radius, -radius) * texelSize, srcLod).rgb;
	result += textureLod(srcTexture, TexCoords + vec2(radius, -radius) * texelSize, srcLod).rgb;
	result += textureLod(srcTexture, TexCoords + vec2(-radius, radius) * texelSize, srcLod).rgb;
	result += textureLod(srcTexture, TexCoords + vec2(radius, radius) * texelSize, srcLod).rgb;

	// Cardinal samples at half-pixel offset (weight 2 each)
	result += textureLod(srcTexture, TexCoords + vec2(0.0, -radius) * texelSize, srcLod).rgb * 2.0;
	result += textureLod(srcTexture, TexCoords + vec2(0.0, radius) * texelSize, srcLod).rgb * 2.0;
	result += textureLod(srcTexture, TexCoords + vec2(-radius, 0.0) * texelSize, srcLod).rgb * 2.0;
	result += textureLod(srcTexture, TexCoords + vec2(radius, 0.0) * texelSize, srcLod).rgb * 2.0;

	FragColor = vec4(result / 16.0, 1.0);
}
