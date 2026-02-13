#version 420 core
layout(location = 0) out vec4 FragHeight;

in vec3  Normal;
in vec3  FragPos;
in vec2  TexCoords;
in float perturbFactor;
in float tessFactor;

void main() {
	// Red channel: world-space height
	// Green channel: normal.y (optional, good for debugging)
	// Blue/Alpha: padding
	FragHeight = vec4(FragPos.y, Normal.y, 0.0, 1.0);
}
