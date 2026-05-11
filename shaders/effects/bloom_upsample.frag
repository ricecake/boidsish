#version 460 core
out vec4 FragColor;

in vec2 vTexCoords;
#define TexCoords vTexCoords

uniform sampler2DArray srcTexture;
uniform vec2      srcResolution;
uniform float     filterRadius;
uniform float     srcLod = 0.0;

void main() {
	vec2  texelSize = 1.0 / srcResolution;
	float radius = filterRadius;
	int layer = gl_Layer;

	vec3 result = vec3(0.0);
	result += textureLod(srcTexture, vec3(TexCoords, layer), srcLod).rgb * 4.0;

	result += textureLod(srcTexture, vec3(TexCoords + vec2(-radius, -radius) * texelSize, layer), srcLod).rgb;
	result += textureLod(srcTexture, vec3(TexCoords + vec2(radius, -radius) * texelSize, layer), srcLod).rgb;
	result += textureLod(srcTexture, vec3(TexCoords + vec2(-radius, radius) * texelSize, layer), srcLod).rgb;
	result += textureLod(srcTexture, vec3(TexCoords + vec2(radius, radius) * texelSize, layer), srcLod).rgb;

	result += textureLod(srcTexture, vec3(TexCoords + vec2(0.0, -radius) * texelSize, layer), srcLod).rgb * 2.0;
	result += textureLod(srcTexture, vec3(TexCoords + vec2(0.0, radius) * texelSize, layer), srcLod).rgb * 2.0;
	result += textureLod(srcTexture, vec3(TexCoords + vec2(-radius, 0.0) * texelSize, layer), srcLod).rgb * 2.0;
	result += textureLod(srcTexture, vec3(TexCoords + vec2(radius, 0.0) * texelSize, layer), srcLod).rgb * 2.0;

	FragColor = vec4(result / 16.0, 1.0);
}
