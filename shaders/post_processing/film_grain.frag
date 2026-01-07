#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform float     intensity;
uniform float     time;

// Simple pseudo-random number generator
float rand(vec2 co) {
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
	vec4  originalColor = texture(screenTexture, TexCoords);
	float noise = (rand(TexCoords + time) - 0.5) * intensity;

	// Add colored noise
	vec3 grainColor = vec3(rand(TexCoords * 1.1 + time), rand(TexCoords * 1.2 + time), rand(TexCoords * 1.3 + time));
	vec3 color = originalColor.rgb + grainColor * noise;

	// Add "stuck pixels" for a more intense effect
	if (rand(TexCoords - time) > 0.99) {
		color = vec3(rand(TexCoords.yx * 2.0 + time));
	}

	FragColor = vec4(color, originalColor.a);
}
