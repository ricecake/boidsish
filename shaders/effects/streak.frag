#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform vec2      direction;
uniform float     offset;

// Optimized 9-tap Gaussian filter (5 samples with linear filtering)
// Weights and offsets for dual-tap Gaussian blur
const float weights[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);
const float offsets[3] = float[](0.0, 1.3846153846, 3.2307692308);

void main() {
	vec2  texelSize = 1.0 / vec2(textureSize(image, 0));
	vec3  result = texture(image, TexCoords).rgb * weights[0];
	vec2  step = direction * texelSize * offset;

	for (int i = 1; i < 3; ++i) {
		result += texture(image, TexCoords + step * offsets[i]).rgb * weights[i];
		result += texture(image, TexCoords - step * offsets[i]).rgb * weights[i];
	}

	FragColor = vec4(result, 1.0);
}
