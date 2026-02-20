#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D srcTexture;

vec3 sampleSafe(vec2 uv) {
    vec3 c = texture(srcTexture, uv).rgb;
    if (any(isnan(c)) || any(isinf(c))) return vec3(0.0);
    return clamp(c, vec3(0.0), vec3(1000.0)); // Boundary to prevent extreme bloom
}

// Dual Kawase blur downsample
// Samples in a 3x3 pattern with wider spacing for better blur quality
void main() {
	vec2 texelSize = 1.0 / vec2(textureSize(srcTexture, 0));

	// Sample center
	vec3 result = sampleSafe(TexCoords) * 4.0;

	// Sample 4 corners at distance 1
	result += sampleSafe(TexCoords + vec2(-1.0, -1.0) * texelSize);
	result += sampleSafe(TexCoords + vec2(1.0, -1.0) * texelSize);
	result += sampleSafe(TexCoords + vec2(-1.0, 1.0) * texelSize);
	result += sampleSafe(TexCoords + vec2(1.0, 1.0) * texelSize);

	// Sample 4 edges at distance 2 for wider blur
	result += sampleSafe(TexCoords + vec2(0.0, -2.0) * texelSize) * 0.5;
	result += sampleSafe(TexCoords + vec2(0.0, 2.0) * texelSize) * 0.5;
	result += sampleSafe(TexCoords + vec2(-2.0, 0.0) * texelSize) * 0.5;
	result += sampleSafe(TexCoords + vec2(2.0, 0.0) * texelSize) * 0.5;

	FragColor = vec4(result / 10.0, 1.0);
}
