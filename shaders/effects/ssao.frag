#version 420 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D gDepth;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 invProjection;

uniform vec2  noiseScale;
uniform float radius = 0.5;
uniform float bias = 0.025;

vec3 getPos(vec2 uv) {
	float depth = texture(gDepth, uv).r;
	vec4  clipSpacePosition = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	return viewSpacePosition.xyz / viewSpacePosition.w;
}

void main() {
	vec3 fragPos = getPos(TexCoords);

    // If depth is too far (skybox), don't do SSAO
    float depth = texture(gDepth, TexCoords).r;
    if (depth >= 0.9999) {
        FragColor = 1.0;
        return;
    }

	// Improved normal reconstruction: Sample 3 points to avoid derivative artifacts
    vec2 texelSize = 1.0 / textureSize(gDepth, 0);
    vec3 posRight = getPos(TexCoords + vec2(texelSize.x, 0.0));
    vec3 posUp = getPos(TexCoords + vec2(0.0, texelSize.y));
    vec3 normal = normalize(cross(posRight - fragPos, posUp - fragPos));

    // Fallback if cross product fails (e.g. at edge)
    if (length(normal) < 0.1) {
        normal = normalize(cross(dFdx(fragPos), dFdy(fragPos)));
    }

	vec3 randomVec = normalize(texture(texNoise, TexCoords * noiseScale).xyz);

	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN = mat3(tangent, bitangent, normal);

	float occlusion = 0.0;

    // Add a small randomized rotation per pixel to further break up banding
    // (In addition to the noise texture)
	for (int i = 0; i < 64; ++i) {
		// get sample position
		vec3 samplePos = TBN * samples[i]; // from tangent to view-space
		samplePos = fragPos + samplePos * radius;

		// project sample position
		vec4 offset = vec4(samplePos, 1.0);
		offset = projection * offset;   // from view to clip-space
		offset.xyz /= offset.w;         // perspective divide
		offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0

		// get sample depth
		float sampleDepth = getPos(offset.xy).z;

		// range check & accumulate
		float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
	}
	occlusion = 1.0 - (occlusion / 64.0);

	FragColor = occlusion;
}
