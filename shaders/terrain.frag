#version 430 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 Velocity;
layout(location = 2) out vec4 NormalOut;
layout(location = 3) out vec4 AlbedoOut;

in vec3       Normal;
in vec3       FragPos;
in vec4       CurPosition;
in vec4       PrevPosition;
in vec2       TexCoords;
flat in float TextureSlice;
in float      vIsWater;
in float      vSubstrate;

#define USE_TERRAIN_DATA
#include "helpers/fast_noise.glsl"
#include "helpers/terrain_noise.glsl"
#include "helpers/terrain_shadows.glsl"
#include "helpers/lighting.glsl"
#include "visual_effects.glsl"

uniform bool uIsShadowPass = false;

#ifndef TERRAIN_HEIGHT_SAMPLERS_DEFINED
#define TERRAIN_HEIGHT_SAMPLERS_DEFINED
uniform sampler2DArray uBiomeMap;
uniform sampler2DArray uBakedHeightNormal;
uniform sampler2DArray uBakedAlbedoRoughness;
uniform sampler2DArray uBakedParams;
#endif
uniform float          uRawChunkSize;

uniform mat4 view;

// Helper to avoid sign(0) == 0
vec2 signNotZero(vec2 v) {
	return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

void main() {
	if (uIsShadowPass) return;

	// Sample baked maps
	vec4 bakedData = texture(uBakedHeightNormal, vec3(TexCoords, TextureSlice));
	vec4 bakedAlbedoRough = texture(uBakedAlbedoRoughness, vec3(TexCoords, TextureSlice));
	vec4 bakedParams = texture(uBakedParams, vec3(TexCoords, TextureSlice));

	// Octahedral decoding (+Y is Up)
	vec2 e = bakedData.gb * 2.0 - 1.0;
	vec3 v = vec3(e.x, 1.0 - abs(e.x) - abs(e.y), e.y);
	if (v.y < 0.0) {
		v.xz = (1.0 - abs(v.zx)) * signNotZero(v.xz);
	}
	vec3 norm = normalize(v);

	if (vIsWater > 0.5) {
		norm = normalize(Normal);
		float grid_spacing = 1.0;
		float rippleHeight = FragPos.y;
		float dist = length(FragPos.xz - viewPos.xz);

		vec2 refractionOffset = norm.xz * abs(rippleHeight) * 4.0;
		if (dist <= 75.0) {
			refractionOffset = fastCurl3d(vec3(norm.xz / 100.0, rippleHeight)).xz * abs(rippleHeight) * 2.0 *
				smoothstep(75.0, 50.0, dist);
		}
		vec2 coord = (FragPos.xz + refractionOffset) / grid_spacing;
		vec2 f = fwidth(coord);
		vec2  grid_minor = abs(fract(coord - 0.5) - 0.5) / f;
		float line_minor = min(grid_minor.x, grid_minor.y);
		float C_minor = 1.0 - min(line_minor, 1.0);
		vec2  grid_major = abs(fract(coord / 5.0 - 0.5) - 0.5) / f;
		float line_major = min(grid_major.x, grid_major.y);
		float C_major = 1.0 - min(line_major, 1.0);

		float shimmer = 1.0 + rippleHeight * 2.0;
		float grid_intensity = max(C_minor, C_major * 1.5) * 0.6 * shimmer;
		vec3  grid_color = vec3(0.0, 0.8, 0.8) * grid_intensity;
		vec3 surfaceColor = vec3(0.05, 0.05, 0.08);

		float primaryShadow;
		vec3 lighting = apply_lighting_pbr(FragPos, norm, surfaceColor, 0.05, 0.9, 1.0, primaryShadow).rgb;
		vec3 final_color = lighting + grid_color;

		float fade_start = 560.0 * worldScale;
		float fade_end = 570.0 * worldScale;
		float n_fade = fastSimplex3d(vec3(FragPos.xz / (250 * worldScale), time * 0.09));
		float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

		vec4 baseColor = vec4(final_color, fade);
		FragColor = mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));
		NormalOut = vec4(normalize(mat3(view) * norm), primaryShadow);
		AlbedoOut = vec4(surfaceColor, 1.0);
		return;
	}

	vec3  albedo = bakedAlbedoRough.rgb;
	float roughness = bakedAlbedoRough.a;
	float metallic = bakedParams.r;

	float dist = length(FragPos.xz - viewPos.xz);
	float n_fade = fastSimplex3d(vec3(FragPos.xz / (250 * worldScale), time * 0.09));
	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	if (fade < 0.2) discard;

	vec3 perturbedNorm = norm;

	float fateFactor = fastWorley3d(vec3(FragPos.xz / 50.0, time * 0.25)) * 0.5 + 0.50;
	vec3  windForce = fastCurl3d(vec3(FragPos.x * 0.0005 + time * 0.00125, FragPos.y * 0.001, FragPos.z * 0.0005 + time * 0.0125));
	vec3 rawWindNudge = (fateFactor * windForce);

	vec3  light_dir = normalize(lights[0].position - FragPos);
	float windDistortion = pow(1 - smoothstep(0, 1, (max(0, dot(vec3(0, 1, 0), perturbedNorm)) * ((1 - dot(rawWindNudge, perturbedNorm)) / 2))), 9.0);
	float windRipple = windDistortion * (tangentGabor(FragPos, norm, -1 * windDistortion * rawWindNudge, time, 0.5, 0.00001, 0.75) * 0.5 + 0.5);

	const vec3 COL_GRASS_LUSH = vec3(0.20, 0.45, 0.15);
	const vec3 COL_GRASS_DRY = vec3(0.45, 0.50, 0.25);
	float grassFactor = smoothstep(0.25, 0.5, max(dot(albedo, COL_GRASS_LUSH), dot(albedo, COL_GRASS_DRY)));
	albedo *= mix(1, mix(1.0, 1.25, windDistortion) * mix(1.0, 1.05, windRipple), grassFactor);
	roughness *= mix(1.25, 1.0, windDistortion) * mix(1, mix(1.5, 1.0, windRipple), grassFactor);

	float primaryShadow;
	vec3 lighting = apply_lighting_pbr(FragPos, perturbedNorm, albedo, roughness, metallic, 1.0, primaryShadow).rgb;

	float gridScale = 0.05;
	vec2  gridUV = FragPos.xz * gridScale;
	vec2  grid = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 1.5);
	float gridLine = 1.0 - smoothstep(0.0, 1.0, min(grid.x, grid.y));
	vec2  gridGlow = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 8.0);
	float gridGlowFactor = 1.0 - smoothstep(0.0, 1.0, min(gridGlow.x, gridGlow.y));

	vec3 cyan = vec3(0.0, 1.0, 1.0);
	vec3 magenta = vec3(1.0, 0.0, 1.0);
	vec3 newLighting = mix(lighting, lighting * vec3(0.4, 0.1, 0.5), 0.7);
	newLighting += gridLine * cyan * 0.8 + gridGlowFactor * magenta * 0.4;
	vec3 gridLight = newLighting;
	newLighting += magenta * smoothstep(0.0, 100.0 * worldScale, FragPos.y) * (0.8 + 0.2 * sin(time * 0.5));

	float nightNoise = fastWorley3d(vec3(FragPos.xy / (25 * worldScale), time * 0.08));
	float nightFade = smoothstep(fade_start - 10, fade_end, dist + nightNoise * 100.0);
	lighting = mix(mix(lighting, gridLight, smoothstep(fade_start - 150, fade_end - 20, dist)), newLighting, nightFade);

	vec4 baseColor = vec4(lighting, mix(0.0, fade, step(0.01, FragPos.y)));
	FragColor = mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));

	NormalOut = vec4(normalize(mat3(view) * perturbedNorm), primaryShadow);
	AlbedoOut = vec4(albedo, 1.0);

	vec2 va = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 vb = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = vec4(va - vb, roughness, metallic);
}
