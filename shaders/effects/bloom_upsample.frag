#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D srcTexture;
uniform vec2      srcResolution;
uniform float     filterRadius; // Default 1.0

vec3 sampleSafe(vec2 uv) {
    vec3 c = texture(srcTexture, uv).rgb;
    if (any(isnan(c)) || any(isinf(c))) return vec3(0.0);
    return c;
}

// Dual Kawase blur upsample with tent filter
// Provides smooth, high-quality upsampling with natural blur
void main() {
	vec2  texelSize = 1.0 / srcResolution;
	float radius = filterRadius;

	// 9-tap tent filter for smooth upsampling
	vec3 result = vec3(0.0);

	// Center sample (weight 4)
	result += sampleSafe(TexCoords) * 4.0;

	// Diagonal samples at half-pixel offset (weight 1 each)
	result += sampleSafe(TexCoords + vec2(-radius, -radius) * texelSize);
	result += sampleSafe(TexCoords + vec2(radius, -radius) * texelSize);
	result += sampleSafe(TexCoords + vec2(-radius, radius) * texelSize);
	result += sampleSafe(TexCoords + vec2(radius, radius) * texelSize);

	// Cardinal samples at half-pixel offset (weight 2 each)
	result += sampleSafe(TexCoords + vec2(0.0, -radius) * texelSize) * 2.0;
	result += sampleSafe(TexCoords + vec2(0.0, radius) * texelSize) * 2.0;
	result += sampleSafe(TexCoords + vec2(-radius, 0.0) * texelSize) * 2.0;
	result += sampleSafe(TexCoords + vec2(radius, 0.0) * texelSize) * 2.0;

	FragColor = vec4(result / 16.0, 1.0);
}
