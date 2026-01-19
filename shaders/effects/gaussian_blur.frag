#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform bool horizontal;

// Using 5 samples for a more scattered, softer bloom, which is less
// prone to washing out the screen. The key is the wider, non-uniform
// spacing of the samples.
const float weights[5] = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);
const float offsets[5] = float[](0.0, 1.3846153846, 3.2307692308, 5.0, 7.0);

void main() {
	vec2 texelSize = 1.0 / textureSize(image, 0);
	vec3 result = texture(image, TexCoords).rgb * weights[0];

	if (horizontal) {
		for (int i = 1; i < 5; ++i) {
			result += texture(image, TexCoords + vec2(texelSize.x * offsets[i], 0.0)).rgb * weights[i];
			result += texture(image, TexCoords - vec2(texelSize.x * offsets[i], 0.0)).rgb * weights[i];
		}
	} else {
		for (int i = 1; i < 5; ++i) {
			result += texture(image, TexCoords + vec2(0.0, texelSize.y * offsets[i])).rgb * weights[i];
			result += texture(image, TexCoords - vec2(0.0, texelSize.y * offsets[i])).rgb * weights[i];
		}
	}

	FragColor = vec4(result, 1.0);
}
