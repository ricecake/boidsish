#version 430 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

in vec3       Normal;
in vec3       FragPos;
in vec4       CurPosition;
in vec4       PrevPosition;
in vec2       TexCoords;
flat in float TextureSlice;
in float      perturbFactor;
in float      tessFactor;

#include "helpers/fast_noise.glsl"
#include "helpers/lighting.glsl"
#include "helpers/terrain_noise.glsl"
#include "visual_effects.glsl"
// #include "helpers/noise.glsl"

uniform bool uIsShadowPass = false;

// Biome texture array: RG8 - R=low_idx, G=t
uniform sampler2DArray uBiomeMap;

// Shading cache textures
uniform sampler2DArray u_shadingCacheA; // albedo.rgb, roughness
uniform sampler2DArray u_shadingCacheB; // normal.rgb, metallic

layout(std430, binding = 14) buffer ShadingStatus {
    int shading_status[];
};

struct BiomeProperties {
	vec4 albedo_roughness; // rgb = albedo, w = roughness
	vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = unused
};

layout(std140, binding = 7) uniform BiomeData {
	BiomeProperties biomes[8];
};

#include "helpers/terrain_material.glsl"

void calculateProceduralMaterial(
	vec3        worldPos,
	vec3        worldNorm,
	vec2        texCoords,
	float       slice,
	float       pFactor,
	out vec3    outAlbedo,
	out float   outRoughness,
	out float   outMetallic,
	out vec3    outPerturbedNorm
) {
	float baseFreq = 0.1 / worldScale;
	float largeNoise = fastWarpedFbm3d(worldPos * (baseFreq * 0.5));
	float medNoise = fastWorley3d(vec3(largeNoise) * (baseFreq * 2.0));
	float fineNoise = fastFbm3d(worldPos * (baseFreq * 5.0));

	float combinedNoise = largeNoise * 0.6 + (1.0 - medNoise) * 0.3 + fineNoise * 0.1;

	// Height with noise distortion for natural boundaries
	float baseHeight = worldPos.y;
	float distortedHeight = baseHeight + largeNoise * 5.0 * worldScale;

	// Slope analysis: 1.0 = flat horizontal, 0.0 = vertical cliff
	float slope = dot(worldNorm, vec3(0.0, 1.0, 0.0));
	float distortedSlope = slope + medNoise * 0.08;

	// Get biome data from texture
	vec2  biomeData = texture(uBiomeMap, vec3(texCoords, slice)).rg;
	int   lowIdx = int(biomeData.r * 255.0 + 0.5);
	int   highIdx = min(lowIdx + 1, 7);
	float t = biomeData.g;

	// Organic biome transitions using warped noise
	float warpScale = 0.05 / worldScale;
	float transitionWarp = fastWarpedFbm3d(worldPos * warpScale);
	float organicT = clamp(t + transitionWarp * 0.4 - 0.2, 0.0, 1.0);
	organicT = smoothstep(0.0, 1.0, organicT);

	BiomeProperties lowBiome = biomes[lowIdx];
	BiomeProperties highBiome = biomes[highIdx];

	outAlbedo = mix(lowBiome.albedo_roughness.rgb, highBiome.albedo_roughness.rgb, organicT);
	outRoughness = mix(lowBiome.albedo_roughness.a, highBiome.albedo_roughness.a, organicT);
	outMetallic = mix(lowBiome.params.x, highBiome.params.x, organicT);

	float verticalMask = smoothstep(0.4, 0.2, slope);
	float valleyFactor = calculateValleyFactor(worldPos);
	float moisture = calculateMoisture(baseHeight, valleyFactor, worldPos);
	float valleyLushness = clamp(-valleyFactor, 0.0, 1.0);
	moisture = mix(moisture, min(moisture + 0.3, 1.0), valleyLushness);

	TerrainMaterial biomeMat = getBiomeMaterial(distortedHeight, moisture, combinedNoise);
	TerrainMaterial cliffMat = getCliffMaterial(baseHeight, medNoise);

	float cliffThreshold = mix(0.4, 0.3, smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, baseHeight));
	float cliffMask = smoothstep(cliffThreshold, cliffThreshold - 0.15, distortedSlope);

	cliffMask = max(cliffMask, verticalMask);
	cliffMask += (medNoise - 0.5) * 0.15;
	cliffMask = clamp(cliffMask, 0.0, 1.0);

	float beachMask = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END + 2.0, baseHeight);
	cliffMask *= (1.0 - beachMask);

	TerrainMaterial finalMaterial;
	finalMaterial.albedo = mix(biomeMat.albedo, cliffMat.albedo, cliffMask);
	finalMaterial.roughness = mix(biomeMat.roughness, cliffMat.roughness, cliffMask);
	finalMaterial.metallic = mix(biomeMat.metallic, cliffMat.metallic, cliffMask);
	finalMaterial.normalScale = mix(biomeMat.normalScale, cliffMat.normalScale, cliffMask);
	finalMaterial.normalStrength = mix(biomeMat.normalStrength, cliffMat.normalStrength, cliffMask);

	outAlbedo = finalMaterial.albedo;
	outRoughness = finalMaterial.roughness;
	outMetallic = finalMaterial.metallic;

	float macroNoise = fastSimplex3d(worldPos * (baseFreq * 0.1));
	outAlbedo *= (1.0 + macroNoise * 0.12);
	outAlbedo *= (1.0 + combinedNoise * 0.15);

	float rockyVar = fastWarpedFbm3d(worldPos * (baseFreq * 4.0));
	float rockyMask = smoothstep(0.5, 0.2, slope);
	outAlbedo = mix(outAlbedo, outAlbedo * (1.0 + rockyVar * 0.2), rockyMask);

	outPerturbedNorm = worldNorm;
	float normalStrength = finalMaterial.normalStrength;
	float normalScale = finalMaterial.normalScale;

	if (pFactor >= 0.1 && normalStrength > 0.0) {
		float roughnessStrength = smoothstep(0.1, 1.0, pFactor) * normalStrength;
		float roughnessScale = normalScale * 0.05;
		vec3  scaledFragPos = worldPos / worldScale;

		float eps = 0.015;
		float n = fastWorley3d(0.1 * scaledFragPos * roughnessScale);
		float nx = fastWorley3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
		float nz = fastWorley3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);

		vec3 tangent = normalize(cross(worldNorm, vec3(0, 0, 1)));
		if (abs(worldNorm.z) > 0.9)
			tangent = normalize(cross(worldNorm, vec3(1, 0, 0)));
		vec3 bitangent = cross(worldNorm, tangent);

		outPerturbedNorm = normalize(worldNorm + (tangent * (n - nx) + bitangent * (n - nz)) * (roughnessStrength / eps));

		float ft = length(outPerturbedNorm);
		ft = clamp(ft, 0.01, 1.0);
		float r2 = outRoughness * outRoughness;
		float newGloss = r2 / (ft * (1.0 + (1.0 - ft) / r2));
		outRoughness = sqrt(newGloss);
	}
}

void main() {
	if (uIsShadowPass) {
		// Output only depth (handled by hardware)
		return;
	}

	// Calculate screen-space velocity
	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = a - b;

	// Distance Fade -- precalc
	float dist = length(FragPos.xz - viewPos.xz);
	// Use texture-based noise on XZ plane for distance fade
	float n_fade = fastSimplex3d(vec3(FragPos.xz / (25.0 * worldScale), time * 0.08));
	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	if (fade < 0.2) {
		discard;
	}

	vec3 albedo;
	float roughness;
	float metallic;
	vec3 perturbedNorm;
	vec3 norm = normalize(Normal);

	int sliceIdx = int(TextureSlice);
	if (shading_status[sliceIdx] == 1) {
		// Use cached shading
		vec4 cacheA = texture(u_shadingCacheA, vec3(TexCoords, TextureSlice));
		vec4 cacheB = texture(u_shadingCacheB, vec3(TexCoords, TextureSlice));
		albedo = cacheA.rgb;
		roughness = cacheA.a;
		metallic = cacheB.a;

		// Re-apply perturbFactor to the baked displacement
		vec3 bakedPerturb = cacheB.rgb;
		perturbedNorm = normalize(norm + bakedPerturb * smoothstep(0.1, 1.0, perturbFactor));

		// Toksvig Factor: Adjust roughness based on normal length after interpolation
		float ft = length(norm + bakedPerturb * smoothstep(0.1, 1.0, perturbFactor));
		ft = clamp(ft, 0.01, 1.0);
		float r2 = roughness * roughness;
		float newGloss = r2 / (ft * (1.0 + (1.0 - ft) / r2));
		roughness = sqrt(newGloss);
	} else {
		calculateProceduralMaterial(FragPos, normalize(Normal), TexCoords, TextureSlice, perturbFactor, albedo, roughness, metallic, perturbedNorm);
	}

	// Final Lighting
	float fateFactor = fastWorley3d(vec3(FragPos.xz / 50.0, time * 0.25)) * 0.5 + 0.50;
	vec3  windForce = fastCurl3d(
        vec3(FragPos.x * 0.0005 + time * 0.00125, FragPos.y * 0.001, FragPos.z * 0.0005 + time * 0.0125)
    );
	vec3 rawWindNudge = (fateFactor * windForce); // / (abs(normalize(FragPos).y - normalize(windForce).y));

	vec3  light_dir = normalize(lights[0].position - FragPos);
	float rim = max(dot(light_dir, normalize(viewPos - FragPos)), 0.0);
	// albedo += (1-dot(rawWindNudge, perturbedNorm)) * rim * albedo;
	float windDistortion = pow(
		1 -
			smoothstep(
				0,
				1,
				(max(0, dot(vec3(0, 1, 0), perturbedNorm)) * ((1 - dot(rawWindNudge, perturbedNorm)) / 2))
			),
		9.0
	);
	float plainRipple = tangentGabor(FragPos, norm, -1 * windDistortion * rawWindNudge, time, 0.5, 0.00001, 0.75) *
			0.5 +
		0.5;
	float windRipple = windDistortion * plainRipple;
	float grassFactor = smoothstep(0.25, 0.5, max(dot(albedo, COL_GRASS_LUSH), dot(albedo, COL_GRASS_DRY)));
	albedo *= mix(1, mix(1.0, 1.25, windDistortion) * mix(1.0, 1.05, windRipple), grassFactor);
	roughness *= mix(1.25, 1.0, windDistortion) * mix(1, mix(1.5, 1.0, windRipple), grassFactor);
	// perturbedNorm += rawWindNudge * mix(0.0, 1.05, plainRipple);

	vec3 lighting = apply_lighting_pbr(FragPos, perturbedNorm, albedo, roughness, metallic, 1.0).rgb;

	// ========================================================================
	// Neon 80s Synth Style (Night Theme)
	// ========================================================================
	// Synthwave grid lines
	float gridScale = 0.05; // Lines every 20 units
	vec2  gridUV = FragPos.xz * gridScale;

	// Use derivative-based anti-aliasing for the grid lines
	vec2  grid = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 1.5);
	float line = min(grid.x, grid.y);
	float gridLine = 1.0 - smoothstep(0.0, 1.0, line);

	// Thicker grid for glow effect
	vec2  gridGlow = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 8.0);
	float lineGlow = min(gridGlow.x, gridGlow.y);
	float gridGlowFactor = 1.0 - smoothstep(0.0, 1.0, lineGlow);

	vec3 cyan = vec3(0.0, 1.0, 1.0);
	vec3 magenta = vec3(1.0, 0.0, 1.0);

	// Blend albedo towards dark purple/magenta for that 80s look
	vec3 newLighting = mix(lighting, lighting * vec3(0.4, 0.1, 0.5), 0.7);

	// Add cyan grid with magenta glow
	newLighting += gridLine * cyan * 0.8;
	newLighting += gridGlowFactor * magenta * 0.4;
	vec3 gridLight = newLighting;

	// Height-based neon pulse/glow
	float heightGlow = smoothstep(0.0, 100.0 * worldScale, FragPos.y);
	newLighting += magenta * heightGlow * (0.8 + 0.2 * sin(time * 0.5));

	float nightNoise = fastWorley3d(vec3(FragPos.xy / (25 * worldScale), time * 0.08));
	float nightFade = smoothstep(fade_start - 10, fade_end, dist + nightNoise * 100.0);
	lighting = mix(mix(lighting, gridLight, smoothstep(fade_start - 150, fade_end - 20, dist)), newLighting, nightFade);

	// ========================================================================
	// Distance Fade
	// ========================================================================
	// The AtmosphereEffect will handle the scattering over the terrain.
	// We just handle the alpha fade for transition to the skybox.
	vec4 baseColor = vec4(lighting, mix(0.0, fade, step(0.01, FragPos.y)));

	// Restore deliberate cyan style for distant terrain
	FragColor = mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));
}
