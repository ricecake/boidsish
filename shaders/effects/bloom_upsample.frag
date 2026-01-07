#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D originalTexture; // The brighter, more detailed texture from the chain
uniform sampler2D blurTexture;     // The softer, more blurred texture from the previous upsample step

void main() {
	vec3 originalColor = texture(originalTexture, TexCoords).rgb;
	vec3 blurColor = texture(blurTexture, TexCoords).rgb;
	FragColor = vec4(originalColor + blurColor, 1.0);
}
