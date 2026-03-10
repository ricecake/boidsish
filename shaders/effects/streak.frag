#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform vec2      direction; // e.g., (1, 0) for horizontal
uniform int       samples;   // Number of samples, e.g., 16 or 32
uniform float     spacing;   // Initial spacing in texels

void main() {
	vec2  texelSize = 1.0 / vec2(textureSize(image, 0));
	vec3  result = texture(image, TexCoords).rgb;
	float weight = 1.0;

	// Use a simple box-like blur but with increasing spacing for a longer streak
	// or a more sophisticated distribution if needed.
	// For a classic "spike" we want it to decay.
	for (int i = 1; i < samples; ++i) {
		float offset = pow(float(i), 1.5) * spacing;
		float currentWeight = 1.0 / float(i + 1);
		result += texture(image, TexCoords + direction * texelSize * offset).rgb * currentWeight;
		result += texture(image, TexCoords - direction * texelSize * offset).rgb * currentWeight;
		weight += 2.0 * currentWeight;
	}

	FragColor = vec4(result / weight, 1.0);
}
