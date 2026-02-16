#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D image;
uniform float offset;

void main() {
	vec2 res = textureSize(image, 0);
	vec2 texelSize = 1.0 / res;

	vec3 result = texture(image, TexCoords + vec2(offset + 0.5, offset + 0.5) * texelSize).rgb;
	result += texture(image, TexCoords + vec2(-offset - 0.5, offset + 0.5) * texelSize).rgb;
	result += texture(image, TexCoords + vec2(offset + 0.5, -offset - 0.5) * texelSize).rgb;
	result += texture(image, TexCoords + vec2(-offset - 0.5, -offset - 0.5) * texelSize).rgb;

	FragColor = vec4(result * 0.25, 1.0);
}
