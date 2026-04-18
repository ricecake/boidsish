#version 430 core
#include "../textures/scene.glsl"
out vec4 FragColor;

in vec2 TexCoords;


void main() {
	FragColor = vec4(vec3(1.0) - texture(sceneTexture, TexCoords).rgb, 1.0);
}
