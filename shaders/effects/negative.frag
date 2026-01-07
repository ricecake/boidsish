#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;

void main() {
	FragColor = vec4(vec3(1.0) - texture(sceneTexture, TexCoords).rgb, 1.0);
}
