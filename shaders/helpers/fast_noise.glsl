#ifndef HELPERS_FAST_NOISE_GLSL
#define HELPERS_FAST_NOISE_GLSL

#include "helpers/noise.glsl"
#include "lygia/generative/pnoise.glsl"
#include "lygia/generative/psrdnoise.glsl"
#include "lygia/generative/random.glsl"


// Helper functions for fast texture-based noise lookups
// Requires noise texture samplers bound to fixed units:
// u_noiseTexture: 3D, unit 5, R=Simplex/G=Worley/B=FBM/A=Warped
// u_curlTexture: 3D, unit 6, RGB=Curl/A=FBM Curl Mag
// u_blueNoiseTexture: 2D, unit 7, RGBA tiling blue noise at 4 frequencies
// u_extraNoiseTexture: 3D, unit 8, R=Ridge/G=Gradient

#ifndef NOISE_TEXTURES_DEFINED
#define NOISE_TEXTURES_DEFINED
uniform sampler3D u_noiseTexture;
uniform sampler3D u_curlTexture;
uniform sampler2D u_blueNoiseTexture;
uniform sampler3D u_extraNoiseTexture;
uniform sampler2D u_phasorTexture;
#endif

// Tile-aware 2D hash
vec2 hash2Tile(vec2 p, vec2 period)
{
	if (period.x > 0.0) p = mod(p, period);
	p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
	return fract(sin(p) * 43758.5453123);
}

vec3 hash3Tile(vec3 p, vec3 period)
{
	if (period.x > 0.0) p = mod(p, period);
    p = vec3( dot(p, vec3(127.1, 311.7, 74.7)),
            dot(p, vec3(269.5, 183.3, 246.1)),
            dot(p, vec3(113.5, 271.9, 124.6)));
    return -1. + 2. * fract(sin(p) * 43758.5453123);
}

// Tile-aware hash returning a single float
float hash12Tile(vec2 p, vec2 period) {
	if (period.x > 0.0) p = mod(p, period);
	vec3 p3 = fract(vec3(p.xyx) * .1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float hash13Tile(in vec3 pos, in vec3 period) {
	if (period.x > 0.0) pos = mod(pos, period);
    pos  = fract(pos * vec3(.1031, .1030, .0973));
    pos += dot(pos, pos.zyx + 31.32);
    return fract((pos.x + pos.y) * pos.z);
}



// Helper to compute curl noise using finite differences with tiling simplex noise
vec3 computeCurl(vec3 p, float period) {
	float e = 0.01;
	vec3  dx = vec3(e, 0.0, 0.0);
	vec3  dy = vec3(0.0, e, 0.0);
	vec3  dz = vec3(0.0, 0.0, e);

	// Offsets to decorrelate the potential field components
	vec3 o1 = vec3(123.4, 567.8, 910.1);
	vec3 o2 = vec3(234.5, 678.9, 112.2);
	vec3 o3 = vec3(345.6, 789.0, 223.3);

	vec3  g;
	float dPsi_z_dy = (psrdnoise(p + dy + o3, vec3(period), 0.0, g) - psrdnoise(p - dy + o3, vec3(period), 0.0, g)) /
		(2.0 * e);
	float dPsi_y_dz = (psrdnoise(p + dz + o2, vec3(period), 0.0, g) - psrdnoise(p - dz + o2, vec3(period), 0.0, g)) /
		(2.0 * e);

	float dPsi_x_dz = (psrdnoise(p + dz + o1, vec3(period), 0.0, g) - psrdnoise(p - dz + o1, vec3(period), 0.0, g)) /
		(2.0 * e);
	float dPsi_z_dx = (psrdnoise(p + dx + o3, vec3(period), 0.0, g) - psrdnoise(p - dx + o3, vec3(period), 0.0, g)) /
		(2.0 * e);

	float dPsi_y_dx = (psrdnoise(p + dx + o2, vec3(period), 0.0, g) - psrdnoise(p - dx + o2, vec3(period), 0.0, g)) /
		(2.0 * e);
	float dPsi_x_dy = (psrdnoise(p + dy + o1, vec3(period), 0.0, g) - psrdnoise(p - dy + o1, vec3(period), 0.0, g)) /
		(2.0 * e);

	return vec3(dPsi_z_dy - dPsi_y_dz, dPsi_x_dz - dPsi_z_dx, dPsi_y_dx - dPsi_x_dy);
}

vec3 computeFbmCurl(vec3 p, int octaves, float period) {
	vec3  total = vec3(0.0);
	float amp = 1.0;
	float freq = 1.0;
	for (int i = 0; i < octaves; i++) {
		total += computeCurl(p * freq, period * freq) * amp;
		amp *= 0.5;
		freq *= 2.0;
	}
	return total;
}

// Tiling 3D Simplex noise wrapper
float simplex3d_tiling(vec3 p, float period) {
	vec3 g;
	return psrdnoise(p, vec3(period), 0.0, g);
}

struct WorleyData3D {
	float f1_dist;
	float f2_dist;
	vec3 f1;
	vec3 f2;
	vec3 p;
};

// Tiling 2D Worley/Cellular noise
WorleyData3D worley3d_tiling_id(vec3 p, vec3 period) {
	vec3  i = floor(p);
	vec3  f = fract(p);
	float minDistSq = 1.0;
	float f2DistSq = 1.0;
	vec3 cellId = vec3(0.0);
	vec3 f2CellId = vec3(0.0);
	for (int z = -1; z <= 1; z++) {
		for (int y = -1; y <= 1; y++) {
			for (int x = -1; x <= 1; x++) {
				vec3 neighbor = vec3(float(x), float(y), float(z));
				// Wrap neighbor + i to [0, period-1]
				vec3 wrapped_coord = mod(i + neighbor, period);
				vec3 point = hash3Tile(wrapped_coord, period);
				vec3 diff = neighbor + point - f;
				float d = dot(diff, diff);
				if (d < minDistSq) {
					f2DistSq = minDistSq;
					f2CellId = cellId;
					minDistSq = d;
					cellId = i + neighbor + point;
				}
				else if (d < f2DistSq) {
					f2DistSq = d;
					f2CellId = i + neighbor + point;
				}
			}
		}
	}
	return WorleyData3D(sqrt(minDistSq), sqrt(f2DistSq), cellId, f2CellId, p);
}

WorleyData3D worley3d_tiling_id(vec3 p, float period) {
	return worley3d_tiling_id(p, vec3(period));
}

// Tiling 3D Worley/Cellular noise
vec2 worley3d_tiling(vec3 p, float period) {
	WorleyData3D res = worley3d_tiling_id(p, vec3(period));

	return vec2(res.f1_dist, psrdnoise(res.f1, vec3(period)) * 0.5 + 0.5);
}

struct WorleyData2D {
	float f1_dist;
	float f2_dist;
	vec2 f1;
	vec2 f2;
	vec2 p;
};

// Tiling 2D Worley/Cellular noise
WorleyData2D worley2d_tiling_id(vec2 p, float period) {
	vec2  i = floor(p);
	vec2  f = fract(p);
	float minDistSq = 1.0;
	float f2DistSq = 1.0;
	vec2 cellId = vec2(0.0);
	vec2 f2CellId = vec2(0.0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 neighbor = vec2(float(x), float(y));
			// Wrap neighbor + i to [0, period-1]
			vec2 wrapped_coord = mod(i + neighbor, period);
			vec2 point = hash2Tile(wrapped_coord, vec2(period));
			vec2 diff = neighbor + point - f;
			float d = dot(diff, diff);
			if (d < minDistSq) {
				f2DistSq = minDistSq;
				f2CellId = cellId;
				minDistSq = d;
				cellId = i + neighbor + point;
			}
			else if (d < f2DistSq) {
				f2DistSq = d;
				f2CellId = i + neighbor + point;
			}
		}
	}

	return WorleyData2D(sqrt(minDistSq), sqrt(f2DistSq), cellId, f2CellId, p);
}

// Tiling 2D Worley/Cellular noise
vec2 worley2d_tiling(vec2 p, float period) {
	WorleyData2D res = worley2d_tiling_id(p, period);

	return vec2(res.f1_dist, psrdnoise(res.f1, vec2(period)) * 0.5 + 0.5);
}


// Tiling 3D FBM Perlin (using pnoise for classic Perlin)
float fbm3d_tiling(vec3 p, float period) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; i++) {
		value += amplitude * pnoise(p, vec3(period));
		p *= 2.0;
		period *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// Tiling 3D Ridge noise (based on Simplex)
float ridge3d_tiling(vec3 p, float period) {
	vec3  g;
	float n = psrdnoise(p, vec3(period), 0.0, g);
	return 1.0 - abs(n);
}

// Tiling 3D Gradient noise (classic Perlin)
float gradient3d_tiling(vec3 p, float period) {
	return pnoise(p, vec3(period)) * 0.5 + 0.5;
}


// R: Simplex 3D
float fastSimplex3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).r * 2.0 - 1.0;
}

// G: Worley 3D
float fastWorley3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).g;
}

// B: FBM 3D
float fastFbm3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).b * 2.0 - 1.0;
}

// A: Warped FBM 3D
float fastWarpedFbm3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).a * 2.0 - 1.0;
}

// Extra Noises (from u_extraNoiseTexture)
// R: Ridge 3D
float fastRidge3d(vec3 p) {
	return textureLod(u_extraNoiseTexture, p, 0.0).r;
}

// G: Gradient 3D
float fastGradient3d(vec3 p) {
	return textureLod(u_extraNoiseTexture, p, 0.0).g;
}

vec2 fastWorley3dID(vec3 p) {
	return textureLod(u_extraNoiseTexture, p, 0.0).ba;
}


// Multi-octave texture FBM
float fastTextureFbm(vec3 p, int octaves) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < octaves; i++) {
		value += amplitude * (textureLod(u_noiseTexture, p, 0.0).r * 2.0 - 1.0);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// Curl Noise lookup
vec3 fastCurl3d(vec3 p) {
	return textureLod(u_curlTexture, p, 0.0).rgb;
}

// FBM Curl magnitude lookup
float fastFbmCurl3d(vec3 p) {
	return textureLod(u_curlTexture, p, 0.0).a;
}

// Blue Noise lookups (at different frequencies)
float fastBlueNoise(vec2 uv, int frequencyIndex) {
	vec4 bn = textureLod(u_blueNoiseTexture, uv, 0.0);
	if (frequencyIndex == 0)
		return bn.r;
	if (frequencyIndex == 1)
		return bn.g;
	if (frequencyIndex == 2)
		return bn.b;
	return bn.a;
}

float fastBlueNoise(vec2 uv) {
	return textureLod(u_blueNoiseTexture, uv, 0.0).r;
}

vec4 fastBlueNoiseAll(vec2 uv) {
	return textureLod(u_blueNoiseTexture, uv, 0.0);
}

// Spatiotemporal Blue Noise lookup using golden ratio shift
// Useful for Monte Carlo integration across frames
float fastSpatiotemporalBlueNoise(vec2 uv, int frequencyIndex, int frameIndex) {
    float bn = fastBlueNoise(uv, frequencyIndex);
    // Golden ratio = 0.61803398875
    return fract(bn + float(frameIndex) * 0.61803398875);
}

// float fastSpatiotemporalBlueNoise(vec2 uv, int frameIndex) {
// 	return fastSpatiotemporalBlueNoise(uv, 0, frameIndex);
// }

vec4 fastSpatiotemporalBlueNoise(vec2 uv, int frameIndex) {
	ivec2 bnSize = textureSize(u_blueNoiseTexture, 0);
	vec2 bnUV = (uv + vec2(frameIndex * 13, frameIndex * 7)) / vec2(bnSize);
	vec4 bn = textureLod(u_blueNoiseTexture, bnUV, 0.0);
    // float bn = fastBlueNoise(uv, frequencyIndex);
    // Golden ratio = 0.61803398875
    // return fract(bn + vec4(2.0*sin(frameIndex*0.5)  * 0.61803398875));
    return fract(bn + vec4(frameIndex)  * 0.61803398875);
}

/**
 * Fast 2D Phasor noise lookup.
 * Performs complex multiplication of baked phasor with runtime phase.
 * Returns the real part of the resulting complex number.
 */
float fastPhasor2d(vec2 uv, float runtimePhase) {
	vec2 baked = textureLod(u_phasorTexture, uv, 0.0).rg;

	// Complex multiplication: (R_baked + i*I_baked) * (cos(phi) + i*sin(phi))
	// Result real part = R_baked * cos(phi) - I_baked * sin(phi)
	float cosPhi = cos(runtimePhase);
	float sinPhi = sin(runtimePhase);

	return baked.x * cosPhi - baked.y * sinPhi;
}

#endif // HELPERS_FAST_NOISE_GLSL
