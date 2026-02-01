#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D ssaoTexture;
uniform float     intensity = 1.0;

void main() {
	vec4  color = texture(sceneTexture, TexCoords);
	float ao = texture(ssaoTexture, TexCoords).r;

	FragColor = vec4(color.rgb * mix(1.0, ao, intensity), color.a);
}
