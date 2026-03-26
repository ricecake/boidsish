#version 430 core

layout(location = 0) out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 Biome;

#include "helpers/lighting.glsl"

struct BiomeProperties {
	vec4 albedo_roughness; // rgb = albedo, w = roughness
	vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = unused
};

layout(std140, binding = 7) uniform BiomeData {
	BiomeProperties biomes[8];
};

void main() {
    vec3 norm = normalize(Normal);

    // Simple biome blending based on indices in Biome.x (low_idx) and Biome.y (t)
    int low_idx = int(Biome.x);
    int high_idx = min(7, low_idx + 1);
    float t = Biome.y;

    vec3 albedo_low = biomes[low_idx].albedo_roughness.rgb;
    vec3 albedo_high = biomes[high_idx].albedo_roughness.rgb;
    vec3 albedo = mix(albedo_low, albedo_high, t);

    float rough_low = biomes[low_idx].albedo_roughness.w;
    float rough_high = biomes[high_idx].albedo_roughness.w;
    float roughness = mix(rough_low, rough_high, t);

    float metal_low = biomes[low_idx].params.x;
    float metal_high = biomes[high_idx].params.x;
    float metallic = mix(metal_low, metal_high, t);

    vec3 lighting = apply_lighting_pbr(FragPos, norm, albedo, roughness, metallic, 1.0).rgb;

    // Add some simple grid lines for visual scale
    vec2 gridUV = FragPos.xz * 0.1;
    vec2 grid = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 1.5);
    float line = min(grid.x, grid.y);
    float gridLine = 1.0 - smoothstep(0.0, 1.0, line);

    lighting += gridLine * vec3(0.0, 0.5, 0.5) * 0.3;

    FragColor = vec4(lighting, 1.0);
}
