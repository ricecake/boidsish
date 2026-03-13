#version 430 core

out float gl_FragDepth;

in vec2 TexCoords;

uniform sampler2DArray shadowMaps;
uniform vec2 texelSize;
uniform vec2 direction; // (1, 0) for horizontal, (0, 1) for vertical
uniform int layer;

void main() {
	float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

	float result = texture(shadowMaps, vec3(TexCoords, float(layer))).r * weights[0];

	for(int i = 1; i < 5; ++i) {
		result += texture(shadowMaps, vec3(TexCoords + direction * texelSize * i, float(layer))).r * weights[i];
		result += texture(shadowMaps, vec3(TexCoords - direction * texelSize * i, float(layer))).r * weights[i];
	}

	gl_FragDepth = result;
}
