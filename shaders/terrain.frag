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
#include "helpers/terrain_material.glsl"

uniform bool uIsShadowPass = false;
uniform int  uTerrainDebugMode = 0; // 0=Normal, 1=Baked, 2=Procedural, 3=LOD

// Baked material textures
uniform sampler2DArray uBakedMaterial; // rgb=albedo, a=roughness
uniform sampler2DArray uBakedNormal;   // rgb=normal, a=metallic

void main() {
	if (uIsShadowPass) {
		// Output only depth (handled by hardware)
		return;
	}

	// Calculate screen-space velocity
	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = a - b;

	// ========================================================================
	// Distance Fade -- precalc
	// ========================================================================
	vec3 norm = normalize(Normal);
	if (norm == vec3(0, 0, 0)) {
        float baseFreq = 0.1 / worldScale;
	    float largeNoise = fastWarpedFbm3d(FragPos * (baseFreq * 0.5));
		norm = normalize(vec3(largeNoise, 1, largeNoise));
	}

	float dist = length(FragPos.xz - viewPos.xz);
	float n_fade = fastSimplex3d(vec3(FragPos.xy / (125 * worldScale), time * 0.09));
	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	if (fade < 0.2) {
		discard;
	}

    // ========================================================================
	// LOD Calculation for Baked Textures
	// ========================================================================
    // Determine if we should use baked texture or procedural material.
    // uBakedMaterial has resolution 512x512 for the chunk.
    // Calculate the texel size of the baked texture in screen space.
    float bakedTextureRes = 512.0;
    vec2 uv_dx = dFdx(TexCoords * bakedTextureRes);
    vec2 uv_dy = dFdy(TexCoords * bakedTextureRes);
    float pixelSizeInBakedTexels = max(length(uv_dx), length(uv_dy));

    // If pixelSizeInBakedTexels > 1.0, it means one screen pixel covers more than one baked texel.
    // In this case, the baked texture resolution is sufficient.
    // If pixelSizeInBakedTexels < 1.0, it means one baked texel covers multiple screen pixels.
    // In this case, we need to transition to procedural material for more detail.
    float lodFactor = smoothstep(0.5, 1.5, pixelSizeInBakedTexels);

    vec3 albedo;
    float roughness;
    float metallic;
    vec3 perturbedNorm;

    if (uTerrainDebugMode == 3) {
        FragColor = vec4(mix(vec3(1, 0, 0), vec3(0, 1, 0), lodFactor), 1.0);
        return;
    }

    if (uTerrainDebugMode == 1 || (uTerrainDebugMode == 0 && lodFactor >= 1.0)) {
        // Use fully baked material
        vec4 bakedMat = texture(uBakedMaterial, vec3(TexCoords, TextureSlice));
        vec4 bakedNormData = texture(uBakedNormal, vec3(TexCoords, TextureSlice));

        albedo = bakedMat.rgb;
        roughness = bakedMat.a;
        perturbedNorm = normalize(bakedNormData.rgb * 2.0 - 1.0);
        metallic = bakedNormData.a;
    } else {
        // Compute procedural material
        CalculatedMaterial procMat = calculateTerrainMaterial(FragPos, norm, TexCoords, TextureSlice, perturbFactor, worldScale);

        if (uTerrainDebugMode == 2 || lodFactor <= 0.0) {
            albedo = procMat.albedo;
            roughness = procMat.roughness;
            perturbedNorm = procMat.normal;
            metallic = procMat.metallic;
        } else {
            // Blend between baked and procedural
            vec4 bakedMat = texture(uBakedMaterial, vec3(TexCoords, TextureSlice));
            vec4 bakedNormData = texture(uBakedNormal, vec3(TexCoords, TextureSlice));

            albedo = mix(procMat.albedo, bakedMat.rgb, lodFactor);
            roughness = mix(procMat.roughness, bakedMat.a, lodFactor);
            metallic = mix(procMat.metallic, bakedNormData.a, lodFactor);
            perturbedNorm = normalize(mix(procMat.normal, bakedNormData.rgb * 2.0 - 1.0, lodFactor));
        }
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
