#version 420 core
out float FragColor;

in vec2 TexCoords;

#include "lygia/generative/random.glsl"

uniform sampler2D gDepth;
uniform sampler2D texNoise;

uniform vec3  samples[64];
uniform mat4  projection;
uniform mat4  invProjection;
uniform vec2  noiseScale;
uniform float radius = 0.5;
uniform float bias = 0.1;

vec3 getPos(vec2 uv) {
	float depth = texture(gDepth, uv).r;
	vec4  clipSpacePosition = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	return viewSpacePosition.xyz / viewSpacePosition.w;
}

// Improved normal reconstruction: use dFdx/dFdy for stability across depth jumps
vec3 reconstructNormal(vec3 p) {
    return normalize(cross(dFdx(p), dFdy(p)));
}

void main() {
	float depth = texture(gDepth, TexCoords).r;
    // Don't process far plane (skybox)
    if (depth >= 0.99999) {
        FragColor = 1.0;
        return;
    }

	vec3 fragPos = getPos(TexCoords);
	vec3 normal  = reconstructNormal(fragPos);

    // Robust normal fallback
    if (any(isnan(normal)) || length(normal) < 0.01) {
        normal = vec3(0.0, 0.0, 1.0); // Facing camera
    }

    // Combine tiled noise with pixel-specific jitter to break up banding
	vec3 randomVec = texture(texNoise, TexCoords * noiseScale).xyz;
    float jitter = random(TexCoords);
    randomVec = normalize(randomVec + normal * (jitter - 0.5) * 0.1);

	vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN       = mat3(tangent, bitangent, normal);

	float occlusion = 0.0;
	for (int i = 0; i < 64; ++i) {
        vec3 samplePos = TBN * samples[i];
		samplePos = fragPos + samplePos * radius;

		vec4 offset = vec4(samplePos, 1.0);
		offset = projection * offset;
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5 + 0.5;

        // Skip samples outside of the screen
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
            continue;
        }

		float sampleDepth = getPos(offset.xy).z;

        // Sharper range check to prevent haloes around foreground objects
		float rangeCheck = smoothstep(1.0, 0.0, abs(fragPos.z - sampleDepth) / radius);
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
	}

    occlusion = 1.0 - (occlusion / 64.0);

    // Distance fade-out to prevent artifacts on horizon
    float dist = length(fragPos);
    float fade = smoothstep(150.0, 300.0, dist);
    occlusion = mix(occlusion, 1.0, fade);

	FragColor = clamp(occlusion, 0.0, 1.0);
}
