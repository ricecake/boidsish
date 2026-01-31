#version 420 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

#include "helpers/lighting.glsl"

uniform bool uIsShadowPass = false;

vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(vec4 r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v) {
	const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
	const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

	// First corner
	vec3 i = floor(v + dot(v, C.yyy));
	vec3 x0 = v - i + dot(i, C.xxx);

	// Other corners
	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min(g.xyz, l.zxy);
	vec3 i2 = max(g.xyz, l.zxy);

	vec3 x1 = x0 - i1 + C.xxx;
	vec3 x2 = x0 - i2 + C.yyy;
	vec3 x3 = x0 - D.yyy;

	// Permutations
	i = mod289(i);
	vec4 p = permute(
		permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x +
		vec4(0.0, i1.x, i2.x, 1.0)
	);

	float n_ = 0.142857142857;
	vec3  ns = n_ * D.wyz - D.xzx;

	vec4 j = p - 49.0 * floor(p * ns.z * ns.z);

	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_);

	vec4 x = x_ * ns.x + ns.yyyy;
	vec4 y = y_ * ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);

	vec4 b0 = vec4(x.xy, y.xy);
	vec4 b1 = vec4(x.zw, y.zw);

	vec4 s0 = floor(b0) * 2.0 + 1.0;
	vec4 s1 = floor(b1) * 2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));

	vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
	vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

	vec3 p0 = vec3(a0.xy, h.x);
	vec3 p1 = vec3(a0.zw, h.y);
	vec3 p2 = vec3(a1.xy, h.z);
	vec3 p3 = vec3(a1.zw, h.w);

	// Normalise gradients
	vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

	// Mix final noise value
	vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
	m = m * m;
	return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

float fbm(vec3 p) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; i++) {
		value += amplitude * snoise(p);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// Higher octave FBM for fine detail
float[6] fbm_detail(vec3 p) {
	float values[6];
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 6; i++) {
		value += amplitude * snoise(p);
		p *= 2.0;
		amplitude *= 0.5;
		values[i] = value;
	}
	return values;
}

// ============================================================================
// Terrain Biome System
// ============================================================================

// Color palette - realistic terrain tones
const vec3 COL_SAND_WET = vec3(0.55, 0.45, 0.35);      // Wet sand near water
const vec3 COL_SAND_DRY = vec3(0.76, 0.70, 0.55);      // Dry beach sand
const vec3 COL_GRASS_LUSH = vec3(0.20, 0.45, 0.15);    // Lush valley grass
const vec3 COL_GRASS_DRY = vec3(0.45, 0.50, 0.25);     // Drier upland grass
const vec3 COL_FOREST = vec3(0.12, 0.28, 0.10);        // Dense forest
const vec3 COL_ALPINE_MEADOW = vec3(0.35, 0.45, 0.25); // High alpine grass
const vec3 COL_ROCK_BROWN = vec3(0.35, 0.30, 0.25);    // Brown cliff rock
const vec3 COL_ROCK_GREY = vec3(0.45, 0.45, 0.48);     // Grey mountain rock
const vec3 COL_ROCK_DARK = vec3(0.25, 0.23, 0.22);     // Dark wet rock
const vec3 COL_SNOW_FRESH = vec3(0.95, 0.97, 1.00);    // Fresh snow
const vec3 COL_SNOW_OLD = vec3(0.85, 0.88, 0.92);      // Older packed snow
const vec3 COL_DIRT = vec3(0.35, 0.25, 0.18);          // Exposed dirt

// Height thresholds (0 = water level, ~100 = typical peaks)
const float HEIGHT_BEACH_END = 3.0;
const float HEIGHT_LOWLAND_END = 20.0;
const float HEIGHT_FOREST_END = 50.0;
const float HEIGHT_ALPINE_START = 60.0;
const float HEIGHT_TREELINE = 80.0;
const float HEIGHT_SNOW_START = 90.0;
const float HEIGHT_PEAK = 100.0;

/**
 * Calculate valley/ridge factor using noise-based curvature approximation.
 * Valleys tend to accumulate moisture and be more lush.
 * Returns: negative = valley (concave), positive = ridge (convex)
 */
float calculateValleyFactor(vec3 pos) {
	// Sample noise at different scales to approximate local curvature
	float scale = 0.02;
	float center = length(pos * scale);

	// Sample neighbors
	float dx = 5.0;
	float north = length((pos + vec3(0, 0, dx)) * scale);
	float south = length((pos - vec3(0, 0, dx)) * scale);
	float east = length((pos + vec3(dx, 0, 0)) * scale);
	float west = length((pos - vec3(dx, 0, 0)) * scale);

	// Laplacian approximation - negative means we're lower than surroundings (valley)
	float laplacian = (north + south + east + west) / 4.0 - center;

	return laplacian * 10.0; // Scale for usability
}

/**
 * Calculate moisture based on height, valley factor, and noise
 */
float calculateMoisture(float height, float valleyFactor, vec3 pos) {
	// Base moisture decreases with altitude (less rain at high elevations)
	float baseMoisture = 1.0 - smoothstep(0.0, HEIGHT_PEAK, height) * 0.6;

	// Valleys are more moist (water collects there)
	float valleyMoisture = clamp(-valleyFactor * 0.5, 0.0, 0.4);

	// Add some noise variation
	float noiseMoisture = fbm(pos * 0.03) * 0.2;

	return clamp(baseMoisture + valleyMoisture + noiseMoisture, 0.0, 1.0);
}

/**
 * Get the base biome color based on height
 */
vec3 getBiomeColor(float height, float moisture, float noise) {
	// Distort height with noise for natural boundaries
	float h = height + noise * 8.0;

	// Beach zone (0 - 3)
	if (h < HEIGHT_BEACH_END) {
		float wetness = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, h);
		return mix(COL_SAND_DRY, COL_SAND_WET, wetness);
	}

	// Lowland zone (3 - 25): grass/meadow, lusher in valleys
	if (h < HEIGHT_LOWLAND_END) {
		float t = smoothstep(HEIGHT_BEACH_END, HEIGHT_LOWLAND_END, h);
		vec3  grassColor = mix(COL_GRASS_LUSH, COL_GRASS_DRY, t * (1.0 - moisture));
		// Blend from sand to grass
		float sandFade = smoothstep(HEIGHT_BEACH_END, HEIGHT_BEACH_END + 5.0, h);
		return mix(COL_SAND_DRY, grassColor, sandFade);
	}

	// Forest zone (25 - 80): trees dominate
	if (h < HEIGHT_FOREST_END) {
		float t = smoothstep(HEIGHT_LOWLAND_END, HEIGHT_FOREST_END, h);
		// More moisture = denser forest
		vec3 forestColor = mix(COL_GRASS_LUSH, COL_FOREST, moisture);
		// Higher = sparser forest
		return mix(forestColor, COL_GRASS_DRY, t * 0.3);
	}

	// Transition to alpine (80 - 100)
	if (h < HEIGHT_ALPINE_START) {
		float t = smoothstep(HEIGHT_FOREST_END, HEIGHT_ALPINE_START, h);
		return mix(COL_FOREST, COL_ALPINE_MEADOW, t);
	}

	// Alpine meadow (100 - 130): above treeline
	if (h < HEIGHT_TREELINE) {
		float t = smoothstep(HEIGHT_ALPINE_START, HEIGHT_TREELINE, h);
		// Grass becomes sparser, more rock showing through
		vec3 alpineColor = mix(COL_ALPINE_MEADOW, COL_ROCK_GREY, t * 0.4);
		return alpineColor;
	}

	// High alpine / rocky (130 - 160)
	if (h < HEIGHT_SNOW_START) {
		float t = smoothstep(HEIGHT_TREELINE, HEIGHT_SNOW_START, h);
		// Mostly rock with patches of hardy vegetation
		vec3 rockColor = mix(COL_ROCK_BROWN, COL_ROCK_GREY, noise * 0.5 + 0.5);
		vec3 patchColor = mix(rockColor, COL_ALPINE_MEADOW, moisture * 0.3);
		// Gradual snow patches appearing
		return mix(patchColor, COL_SNOW_OLD, t * 0.3);
	}

	// Snow zone (160+)
	float t = smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, h);
	// Higher = fresher/whiter snow
	vec3 snowColor = mix(COL_SNOW_OLD, COL_SNOW_FRESH, t);
	// Some rock still pokes through at lower snow zone
	float rockShow = (1.0 - t) * 0.2 * (1.0 - moisture);
	return mix(snowColor, COL_ROCK_GREY, rockShow);
}

/**
 * Calculate cliff/steep surface coloring
 * Steep surfaces are barren rock, with color varying by altitude
 */
vec3 getCliffColor(float height, float noise) {
	float h = height + noise * 5.0;

	// Low altitude cliffs: brown/dark rock (often wet)
	if (h < HEIGHT_FOREST_END) {
		float wetness = 0.3 + noise * 0.2;
		return mix(COL_ROCK_BROWN, COL_ROCK_DARK, wetness);
	}

	// Mid altitude: mixed brown/grey
	if (h < HEIGHT_SNOW_START) {
		float t = smoothstep(HEIGHT_FOREST_END, HEIGHT_SNOW_START, h);
		return mix(COL_ROCK_BROWN, COL_ROCK_GREY, t + noise * 0.2);
	}

	// High altitude cliffs: grey rock with snow patches
	float snowPatch = smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, h) * 0.4;
	vec3  highRock = mix(COL_ROCK_GREY, COL_ROCK_DARK, noise * 0.3);
	return mix(highRock, COL_SNOW_OLD, snowPatch);
}

void main() {
	if (uIsShadowPass) {
		// Output only depth (handled by hardware)
		return;
	}

	// ========================================================================
	// Noise Generation
	// ========================================================================
	float[6] noise = fbm_detail(FragPos * 0.2);

	// Large scale domain warping for natural variation
	// vec3  warp = vec3(fbm(FragPos * 0.01 + FragPos.x * 0.02));
	vec3 warp = vec3(noise[3]);
	float largeNoise = fbm(FragPos * 0.015 + warp * 0.3);

	// Medium scale noise for biome boundaries
	float medNoise = noise[4];//fbm_detail(FragPos * 0.05);

	// Fine detail noise for texture
	// float fineNoise = fbm_detail(FragPos * 0.2);
	float fineNoise = noise[5];

	// Combined noise for various effects
	float combinedNoise = largeNoise * 0.6 + medNoise * 0.3 + fineNoise * 0.1;


	// float n1 = snoise(vec3(FragPos.xy / 5, FragPos.z * 0.25));
	float n2 = snoise(vec3(FragPos.xy / 25, time * 0.08));
	// float n3 = snoise(vec3(FragPos.xy / 50, FragPos.x * 0.04));
	// ========================================================================
	// Distance Fade -- precalc
	// ========================================================================
	float dist = length(FragPos.xz - viewPos.xz);
	float fade_start = 560.0;
	float fade_end = 570.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n2 * 40.0);

	if (fade < 0.2) {
		discard;
	}

	vec3 norm = normalize(Normal);
	/*
/*
	    // ========================================================================
	    // Terrain Analysis
	    // ========================================================================
*/

		// float largeNoise = n1;
		// float medNoise =  n2;
		// float fineNoise = n3;
		// float combinedNoise = largeNoise * 0.6 + medNoise * 0.3 + fineNoise * 0.1;
	    // Height with noise distortion for natural boundaries
	    float baseHeight = FragPos.y;
	    float distortedHeight = baseHeight + largeNoise * 5.0;

	    // Slope analysis: 1.0 = flat horizontal, 0.0 = vertical cliff
	    float slope = dot(norm, vec3(0.0, 1.0, 0.0));
	    float distortedSlope = slope + medNoise * 0.08;

	    // Valley/ridge detection
	    float valleyFactor = calculateValleyFactor(FragPos);

	    // Moisture calculation
	    float moisture = calculateMoisture(baseHeight, valleyFactor, FragPos);

	    // Valley lushness boost
	    float valleyLushness = clamp(-valleyFactor, 0.0, 1.0);
	    moisture = mix(moisture, min(moisture + 0.3, 1.0), valleyLushness);

	    // ========================================================================
	    // Color Calculation
	    // ========================================================================

	    // Get base biome color
	    vec3 biomeColor = getBiomeColor(distortedHeight, moisture, combinedNoise);

	    // Get cliff color
	    vec3 cliffColor = getCliffColor(baseHeight, combinedNoise);

	    // Cliff mask: steep surfaces become rocky
	    // Threshold varies with altitude (snow sticks to steeper surfaces at high alt)
	    // Lower threshold = only steeper surfaces become cliffs (0.5 = ~60° from horizontal)
	    float cliffThreshold = mix(0.4, 0.3, smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, baseHeight));
	    float cliffMask = smoothstep(cliffThreshold, cliffThreshold - 0.15, distortedSlope);

	    // Near-vertical surfaces (slope < 0.2, ~78° from horizontal) are always cliff-like
	    float verticalMask = smoothstep(0.25, 0.1, slope);
	    cliffMask = max(cliffMask, verticalMask);

	    // Add noise to cliff boundaries for natural look
	    cliffMask += (medNoise - 0.5) * 0.15;
	    cliffMask = clamp(cliffMask, 0.0, 1.0);

	    // Don't make beach areas into cliffs
	    float beachMask = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END + 2.0, baseHeight);
	    cliffMask *= (1.0 - beachMask);

	    // Blend biome with cliff
	    vec3 finalAlbedo = mix(biomeColor, cliffColor, cliffMask);
	    // ========================================================================
	    // Detail Variation
	    // ========================================================================

	    // Add subtle color variation based on fine noise
	    float colorVar = fineNoise * 0.1;
	    finalAlbedo *= (1.0 + colorVar);

	    // Darken valleys slightly (shadow accumulation)
	    float valleyShadow = clamp(-valleyFactor * 0.15, 0.0, 0.1);
	    finalAlbedo *= (1.0 - valleyShadow);

	    // ========================================================================
	    // Lighting
	    // ========================================================================
	// */

	// vec3  warp = vec3(fbm(FragPos / 50 + time * 0.08));
	// float nebula_noise = fbm(FragPos / 50 + warp * 0.8);
	// float funky_noise = fbm(FragPos / 20 + warp.zxy * 1.8);

	// vec3 finalAlbedo = getBiomeColor(FragPos.y + n1, n1+n3, n3);

	//    vec3 bonusColor = mix(vec3(0.1, 0.4, 0.2), vec3(0.1, 0.5, 0.2), FragPos.y / 100);
	//    finalAlbedo = mix(finalAlbedo, bonusColor, nebula_noise);
	// finalAlbedo = mix(vec3(0.2), finalAlbedo, 5*dot(norm, vec3(0,1,0)));
	//    finalAlbedo = mix(finalAlbedo, vec3(1,0.5,0.25), fwidth(norm));
	// finalAlbedo = mix(finalAlbedo, vec3(1,0.5,0.25), fwidth(norm)); // need something that can be a biome specific
	// "flower" Curvature highlighting - clamped and dampened to avoid extreme artifacts in depressions Curvature
	// highlighting - clamped and significantly dampened to avoid extreme artifacts in depressions fwidth(norm) can be
	// noisy on steep tessellated slopes, contributing to "leopard print" artifacts.
	//    float curvature = clamp(length(fwidth(norm)), 0.0, 1.0);
	//    finalAlbedo = mix(finalAlbedo, vec3(1,0.5,0.25), curvature * 0.05);

	// vec3 lighting = apply_lighting(FragPos, norm, finalAlbedo, 0.8);
	vec3 lighting = apply_lighting_pbr(FragPos, norm, finalAlbedo, 0.5, 0.1, 1.0).rgb;

	// ========================================================================
	// Distance Fade
	// ========================================================================
	vec4 outColor = vec4(lighting, mix(0.0, fade, step(0.01, FragPos.y))) + vec4(smoothstep(0.2, 0.5, 1 - fade));
	FragColor = mix(
		vec4(0.0, 0.7, 0.7, mix(0.0, fade, step(0.01, FragPos.y))) * length(outColor),
		outColor,
		step(1.0, fade)
	);

	// vec4 outColor = vec4(lighting, fade);
	// FragColor = mix(
	//      vec4(0.0, 0.7, 0.7, fade) * length(outColor),
	//      outColor,
	//      step(1.0, fade)
	// );
}
