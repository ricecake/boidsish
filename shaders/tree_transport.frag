#version 460 core

in vec3 tePos;
in vec3 teNormal;
in vec2 teTexCoords;
in vec3 teMetadata;

#include "lighting.glsl"
#include "helpers/lighting.glsl"

out vec4 FragColor;

#include "helpers/noise.glsl"
#include "helpers/fast_noise.glsl"

// Simple hash for random values
float hash(uint x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return float(x) / 4294967295.0;
}

void main() {
    vec3 baseColor;
    if (teMetadata.b > 0.5) { // Leaf
        baseColor = vec3(0.1, 0.45, 0.1);
        float variability = hash(uint(teMetadata.g * 1000.0)) * 0.2 - 0.1;
        baseColor += variability;
    } else { // Bark
        baseColor = vec3(0.3, 0.2, 0.15);
        float detail = fastWorley3d(tePos * 10.0).r;
        baseColor *= (0.8 + 0.4 * detail);
    }

    // PBR-lite shading
    vec3 N = normalize(teNormal);
    vec3 L = normalize(vec3(0.5, 1.0, 0.5));
    float diff = max(dot(N, L), 0.0);

    vec3 ambient = baseColor * 0.2;
    vec3 diffuse = baseColor * diff;

    // Simple leaf translucency
    if (teMetadata.b > 0.5) {
        float translucency = max(dot(-N, L), 0.0) * 0.3;
        diffuse += baseColor * translucency;
    }

    FragColor = vec4(ambient + diffuse, 1.0);
}
