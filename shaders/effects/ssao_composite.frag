#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D ssaoTexture;
uniform float     intensity = 1.0;

void main() {
	vec4  color = texture(sceneTexture, TexCoords);
	float ao = texture(ssaoTexture, TexCoords).r;

    // Ensure the multiplier never exceeds 1.0 to prevent highlighting
    float multiplier = clamp(mix(1.0, ao, intensity), 0.0, 1.0);

	FragColor = vec4(color.rgb * multiplier, color.a);
}
