#version 420 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D ssaoInput;
uniform sampler2D gDepth;
uniform mat4      invProjection;

vec3 getPos(vec2 uv) {
	float depth = texture(gDepth, uv).r;
	vec4  clipSpacePosition = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	return viewSpacePosition.xyz / viewSpacePosition.w;
}

void main() {
	vec2  texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
	float result = 0.0;
	vec3  centerPos = getPos(TexCoords);
	float weightSum = 0.0;

	for (int x = -2; x <= 2; ++x) {
		for (int y = -2; y <= 2; ++y) {
			vec2  offset = vec2(float(x), float(y)) * texelSize;
			float sampleAO = texture(ssaoInput, TexCoords + offset).r;
			vec3  samplePos = getPos(TexCoords + offset);

			// Bilateral weight: only blur if samples are at similar depth
			float weight = 1.0 / (0.0001 + abs(centerPos.z - samplePos.z));
			result += sampleAO * weight;
			weightSum += weight;
		}
	}
	FragColor = result / weightSum;
}
