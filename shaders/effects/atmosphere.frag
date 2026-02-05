#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D transmittanceLUT;
uniform sampler3D cloudNoiseLUT;

uniform vec3 cameraPos;
uniform mat4 invView;
uniform mat4 invProjection;
// uniform float time;

uniform float hazeDensity;
uniform float hazeHeight;
uniform vec3  hazeColor;
uniform float cloudDensity;
uniform float cloudAltitude;
uniform float cloudThickness;
uniform vec3  cloudColorUniform;

// New uniforms for enhanced clouds and atmosphere
uniform float hazeG = 0.7;
uniform float cloudG = 0.8;
uniform float cloudScatteringBoost = 2.0;
uniform float cloudPowderStrength = 0.5;

#include "../helpers/atmosphere.glsl"
#include "../helpers/lighting.glsl"
#include "../helpers/noise.glsl"
#include "lygia/generative/worley.glsl"

float getHeightFog(vec3 start, vec3 end, float density, float heightFalloff) {
	float dist = length(end - start);
	vec3  dir = (end - start) / dist;

	float fog;
	if (abs(dir.y) < 0.0001) {
		fog = density * exp(-heightFalloff * start.y) * dist;
	} else {
		fog = (density / (heightFalloff * dir.y)) * (exp(-heightFalloff * start.y) - exp(-heightFalloff * end.y));
	}
	return 1.0 - exp(-max(0.0, fog));
}

/*
float cloudFBM(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    vec3  shift = vec3(time * 0.05, 0.0, time * 0.02);
    for (int i = 0; i < 3; i++) {
        v += a * worley(p + shift);
        p = p * 2.0;
        a *= 0.5;
    }
    return v;
}



// #include "lygia/generative/snoise.glsl"

// Helper: Remap values to control cloud density/erosion

// 1. CUSTOM TILED WORLEY
// Lygia's standard worley doesn't expose the tile period, so we implement a
// grid-wrapping version here.
// p: coordinate
// tile: integer repetition frequency (e.g., 4.0, 8.0)
float worleyTiled(vec3 p, float tile) {
    vec3 id = floor(p);
    vec3 fd = fract(p);

    float minDist = 1.0;

    // Search neighbors
    for (int k = -1; k <= 1; k++) {
        for (int j = -1; j <= 1; j++) {
            for (int i = -1; i <= 1; i++) {
                vec3 neighbor = vec3(float(i), float(j), float(k));

                // CRITICAL: Wrap the neighbor lookup for seamless tiling
                // This ensures the point at (0,0,0) matches (tile, tile, tile)
                vec3 uv = id + neighbor;
                uv = mod(uv, tile); // Wrap the grid ID

                // Hash the wrapped grid ID to get a random point offset
                // (Using a simple hash for portability, or use lygia's hash)
                vec3 pointOffset = fract(sin(vec3(dot(uv, vec3(127.1, 311.7, 74.7)),
                                                  dot(uv, vec3(269.5, 183.3, 246.1)),
                                                  dot(uv, vec3(113.5, 271.9, 124.6)))) * 43758.5453);

                // Animate pointOffset here with time if you want internal motion
                // pointOffset = 0.5 + 0.5 * sin(u_time + 6.2831 * pointOffset);

                vec3 diff = neighbor + pointOffset - fd;
                float dist = length(diff);
                minDist = min(minDist, dist);
            }
        }
    }

    // Invert so 1.0 is the center of the cell (billowy)
    return 1.0 - minDist;
}

// 2. FBM CONSTRUCTION
float getCloudNoise(vec3 uv, float tileFreq) {
    // A. Base Cloud Shape (Worley FBM)
    // We layer 3 octaves of Worley noise.
    float w1 = worleyTiled(uv * tileFreq, tileFreq);
    float w2 = worleyTiled(uv * tileFreq * 2.0, tileFreq * 2.0);
    float w3 = worleyTiled(uv * tileFreq * 4.0, tileFreq * 4.0);

    // FBM: Combine with diminishing weights (0.625, 0.25, 0.125)
    float worleyFBM = w1 * 0.625 + w2 * 0.25 + w3 * 0.125;

    // B. Smooth Irregularity (Simplex Noise)
    // We use Simplex to "distort" or "erode" the Worley base.
    // Ensure the simplex frequency is also a multiple of your base tile if you want
    // strict looping, though high-freq simplex often hides seams well enough.
    float simplex = snoise(uv * tileFreq * 2.0);
    simplex = simplex * 0.5 + 0.5; // Remap to 0-1

    // C. The "Perlin-Worley" Mix
    // We use the Simplex noise to erode the Worley noise.
    // As simplex increases, we push the worley value down.
    float finalNoise = remap(worleyFBM, simplex * 0.3, 1.0, 0.0, 1.0);

    return clamp(finalNoise, 0.0, 1.0);
}

float sampleCloudDensity(vec3 p) {
    float h = (p.y - cloudAltitude) / max(cloudThickness, 0.001);
    float tapering = smoothstep(0.0, 0.2, h) * smoothstep(1.0, 0.5, h);

    vec3  uvw = p * 0.002 + vec3(time * 0.009, time*0.003, time * 0.002);
    // vec4  noise = texture(cloudNoiseLUT, uvw);

    // FBM from 3D texture
    // float d_noise = noise.r * 0.5 + noise.g * 0.25 + noise.b * 0.125 + noise.a * 0.0625;

    // float d_noise = cloudFBM(p * 0.02);
    float d_noise = getCloudNoise(uvw, 5.0);

    float d = smoothstep(0.2, 0.8, d_noise) * cloudDensity * tapering;
    return d;
}

*/

float remap(float value, float original_min, float original_max, float new_min, float new_max) {
	return new_min + (((value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

// 1. CUSTOM TILED WORLEY
// Lygia's standard worley doesn't expose the tile period, so we implement a
// grid-wrapping version here.
// p: coordinate
// tile: integer repetition frequency (e.g., 4.0, 8.0)
float worleyTiled(vec3 p, float tile) {
	vec3 id = floor(p);
	vec3 fd = fract(p);

	float minDist = 1.0;

	// Search neighbors
	for (int k = -1; k <= 1; k++) {
		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {
				vec3 neighbor = vec3(float(i), float(j), float(k));

				// CRITICAL: Wrap the neighbor lookup for seamless tiling
				// This ensures the point at (0,0,0) matches (tile, tile, tile)
				vec3 uv = id + neighbor;
				uv = mod(uv, tile); // Wrap the grid ID

				// Hash the wrapped grid ID to get a random point offset
				// (Using a simple hash for portability, or use lygia's hash)
				vec3 pointOffset = fract(
					sin(vec3(
						dot(uv, vec3(127.1, 311.7, 74.7)),
						dot(uv, vec3(269.5, 183.3, 246.1)),
						dot(uv, vec3(113.5, 271.9, 124.6))
					)) *
					43758.5453
				);

				// Animate pointOffset here with time if you want internal motion
				// pointOffset = 0.5 + 0.5 * sin(u_time + 6.2831 * pointOffset);

				vec3  diff = neighbor + pointOffset - fd;
				float dist = length(diff);
				minDist = min(minDist, dist);
			}
		}
	}

	// Invert so 1.0 is the center of the cell (billowy)
	return clamp(1.0 - minDist, 0.0, 1.0);
}

// A simple Tiled Gradient Noise (Perlin-like)
// p: coordinate, tile: repeat frequency

// Hash function that wraps 'i' on the 'tile' boundary
vec3 wrapHash(vec3 p, float tile) {
	p = mod(p, tile); // <--- FORCE WRAP
	return fract(
			   sin(vec3(
				   dot(p, vec3(127.1, 311.7, 74.7)),
				   dot(p, vec3(269.5, 183.3, 246.1)),
				   dot(p, vec3(113.5, 271.9, 124.6))
			   )) *
			   43758.5453
		   ) *
		2.0 -
		1.0;
}

float gradientNoiseTiled(vec3 p, float tile) {
	vec3 i = floor(p);
	vec3 f = fract(p);

	// Quintic interpolation (smoother than cubic)
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

	// 8 Corners of the cube
	return mix(
		mix(mix(dot(wrapHash(i + vec3(0, 0, 0), tile), f - vec3(0, 0, 0)),
	            dot(wrapHash(i + vec3(1, 0, 0), tile), f - vec3(1, 0, 0)),
	            u.x),
	        mix(dot(wrapHash(i + vec3(0, 1, 0), tile), f - vec3(0, 1, 0)),
	            dot(wrapHash(i + vec3(1, 1, 0), tile), f - vec3(1, 1, 0)),
	            u.x),
	        u.y),
		mix(mix(dot(wrapHash(i + vec3(0, 0, 1), tile), f - vec3(0, 0, 1)),
	            dot(wrapHash(i + vec3(1, 0, 1), tile), f - vec3(1, 0, 1)),
	            u.x),
	        mix(dot(wrapHash(i + vec3(0, 1, 1), tile), f - vec3(0, 1, 1)),
	            dot(wrapHash(i + vec3(1, 1, 1), tile), f - vec3(1, 1, 1)),
	            u.x),
	        u.y),
		u.z
	);
}

// 2. FBM CONSTRUCTION
float getCloudNoise(vec3 uv, float tileFreq) {
	// A. Base Cloud Shape (Worley FBM)
	// We layer 3 octaves of Worley noise.
	float w1 = worleyTiled(uv * tileFreq, tileFreq);
	float w2 = worleyTiled(uv * tileFreq * 2.0, tileFreq * 2.0);
	float w3 = worleyTiled(uv * tileFreq * 4.0, tileFreq * 4.0);

	// FBM: Combine with diminishing weights (0.625, 0.25, 0.125)
	float worleyFBM = w1 * 0.625 + w2 * 0.25 + w3 * 0.125;

	// B. Smooth Irregularity (Simplex Noise)
	// We use Simplex to "distort" or "erode" the Worley base.
	// Ensure the simplex frequency is also a multiple of your base tile if you want
	// strict looping, though high-freq simplex often hides seams well enough.
	float simplex = snoise(uv * tileFreq * 2.0);
	simplex = simplex * 0.5 + 0.5; // Remap to 0-1

	// C. The "Perlin-Worley" Mix
	// We use the Simplex noise to erode the Worley noise.
	// As simplex increases, we push the worley value down.
	float finalNoise = remap(worleyFBM, simplex * 0.3, 1.0, 0.0, 1.0);

	return clamp(finalNoise, 0.0, 1.0);
}

vec4 generatePackedNoise(vec3 uv) {
	float lowFreq = worleyTiled(uv * 4.0, 4.0);
	float midFreq = worleyTiled(uv * 8.0, 8.0);
	float highFreq = worleyTiled(uv * 16.0, 16.0);
	// float simplex = snoise(uv * 4.0);
	// simplex = simplex * 0.5 + 0.5; // Remap -1..1 to 0..1

	// float simplex = 1.0 - worleyTiled(uv * 4.0, 4.0);
	// simplex = simplex * 0.5 + 0.5;

	float simplex = gradientNoiseTiled(uv * 4.0, 4.0); // Returns -1..1
	simplex = simplex * 0.5 + 0.5;                     // Remap to 0..1

	return vec4(lowFreq, midFreq, highFreq, simplex);
}

float sampleCloudDensity(vec3 p) {
	// Sample the packed texture once
	// vec3 uvw = p * vec3(0.001, 0.05*sin(time*length(p.zx)*0.0004), sin(time*0.02)*0.002+0.001);
	vec3 uvw = p * vec3(0.001, 0.05, 0.001);
	uvw += vec3(time * 0.01, 0.0, 0.0);

	uvw = fract(uvw);
	vec4 noiseData = texture(cloudNoiseLUT, uvw);
	// vec4  noiseData = generatePackedNoise(uvw);
	float u_erosionScale = 0.6; // e.g., 0.5
	float u_threshold = 0.5;    // e.g., 0.2 (cloud coverage)

	float baseCloud = noiseData.r;  // Low freq
	float midDetail = noiseData.g;  // Mid freq
	float highDetail = noiseData.b; // High freq

	// 1. Build the detail FBM dynamically
	// You can adjust these weights (0.625, 0.25, 0.125) to change detail roughness
	float detailFBM = midDetail * 0.525 + highDetail * 0.35 + (noiseData.a) * 0.125;

	// 2. Remap (Erosion)
	// We use the detailFBM to "eat away" at the base cloud shape.
	// As u_erosionScale increases, the cloud gets wispier.
	float finalDensity = remap(baseCloud, detailFBM * u_erosionScale, 1.0, 0.0, 1.0);

	// 3. Apply coverage threshold
	return clamp(finalDensity - u_threshold, 0.0, 1.0);
}

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;

	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3 worldPos = (invView * viewSpacePosition).xyz;

	vec3  rayDir = normalize(worldPos - cameraPos);
	float dist = length(worldPos - cameraPos);

	if (depth == 1.0) {
		dist = 5000.0; // Assume sky is far
		worldPos = cameraPos + rayDir * dist;
	}

	// 1. Height Fog (Haze)
	float fogFactor = getHeightFog(cameraPos, worldPos, hazeDensity, 1.0 / (hazeHeight + 0.001));
	vec3  currentHazeColor = hazeColor;

	// Add light scattering to fog
	vec3 scattering = vec3(0.0);
	for (int i = 0; i < num_lights; i++) {
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			vec3  lightDir = normalize(-lights[i].direction);
			float s = henyeyGreensteinPhase(dot(lightDir, rayDir), hazeG);
			scattering += lights[i].color * s * lights[i].intensity * 0.05;
		}
	}
	currentHazeColor += scattering;

	// 2. Cloud Layer
	float cloudTransmittance = 1.0;
	vec3  cloudLight = vec3(0.0);

	// Intersect with cloud layer (volume approximation)
	float t_start = (cloudAltitude - cameraPos.y) / (rayDir.y + 0.000001);
	float t_end = (cloudAltitude + cloudThickness - cameraPos.y) / (rayDir.y + 0.000001);

	if (t_start > t_end) {
		float temp = t_start;
		t_start = t_end;
		t_end = temp;
	}

	t_start = max(t_start, 0.0);
	t_end = min(t_end, dist);

	if (t_start < t_end) {
		int   samples = 32;
		float stepSize = (t_end - t_start) / float(samples);
		float jitter = fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453);

		vec3 sunDir = vec3(0, 1, 0);
		vec3 sunColor = vec3(1);
		for (int i = 0; i < num_lights; i++) {
			if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
				sunDir = normalize(-lights[i].direction);
				sunColor = lights[i].color * lights[i].intensity;
				break;
			}
		}

		for (int i = 0; i < samples; i++) {
			float t = t_start + (float(i) + jitter) * stepSize;
			vec3  p = cameraPos + rayDir * t;
			float d = sampleCloudDensity(p);

			if (d > 0.001) {
				// Self-shadowing using Transmittance LUT
				vec3 T_sun = getTransmittance(transmittanceLUT, p.y, sunDir.y);

				// Beer-Powder Law for internal density and silver lining
				float powder = beerPowder(d, stepSize, cloudPowderStrength);
				vec3  effectiveTransmittance = T_sun * powder;

				// Scattering
				float phase = henyeyGreensteinPhase(dot(rayDir, sunDir), cloudG);
				vec3  pointLight = sunColor * effectiveTransmittance * phase * cloudScatteringBoost;
				pointLight += ambient_light; // Ambient light in clouds

				vec3 src = pointLight * d * cloudColorUniform;
				cloudLight += src * cloudTransmittance * stepSize;
				cloudTransmittance *= exp(-d * stepSize);

				if (cloudTransmittance < 0.01)
					break;
			}
		}
	}

	// Combine everything
	vec3 result = sceneColor * cloudTransmittance + cloudLight;
	result = mix(result, currentHazeColor, fogFactor);

	FragColor = vec4(result, 1.0);
	// FragColor = vec4(vec3(sampleCloudDensity(fract(worldPos))), 1.0);
}
