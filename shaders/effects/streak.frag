#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform vec2      direction;
uniform float     offset;

// High-quality Gaussian blur for hierarchical spikes
// Using 9 samples for a wider, smoother blur per mip
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
	vec2  texelSize = 1.0 / vec2(textureSize(image, 0));
	vec3  result = texture(image, TexCoords).rgb * weights[0];
	vec2  step = direction * texelSize * offset;

	for (int i = 1; i < 5; ++i) {
		result += texture(image, TexCoords + step * float(i)).rgb * weights[i];
		result += texture(image, TexCoords - step * float(i)).rgb * weights[i];
	}

	FragColor = vec4(result, 1.0);
}
