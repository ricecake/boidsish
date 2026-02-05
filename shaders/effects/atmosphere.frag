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

float remap(float value, float original_min, float original_max, float new_min, float new_max) {
	return new_min + (((value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

// Returns a value between 0.0 and 1.0 based on where we are in the cloud layer
// heightFraction: 0.0 = bottom of cloud layer, 1.0 = top of cloud layer
float getCloudGradient(float heightFraction) {
	// REMAP height to a "Cumulus" shape:
	// 1. Bottom (0.0 to 0.1): Sharp fade-in (Flat bottom)
	// 2. Middle (0.1 to 0.9): Full density (Body)
	// 3. Top (0.9 to 1.0): Round fade-out (Puffy top)

	// Simple gradient approach:
	// Create a round "hump" but skew it so the bottom is harder
	float bottom = smoothstep(0.0, 0.1, heightFraction);    // Sharp bottom
	float top = 1.0 - smoothstep(0.6, 1.0, heightFraction); // Soft rounded top

	// Multiply them to create the "bracket"
	return bottom * top;
}

/*
float sampleCloudDensity(vec3 p) {
    // Sample the packed texture once
    // vec3 uvw = p * vec3(0.001, 0.05*sin(time*length(p.zx)*0.0004), sin(time*0.02)*0.002+0.001);
    vec3 uvw = p * vec3(0.001, 0.00035, 0.001);

    uvw += vec3(time * 0.01, 0.0, 0.0);

    uvw = fract(uvw);
    vec4 noiseData = texture(cloudNoiseLUT, uvw);

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
*/

/*
float sampleCloudDensity(vec3 p) {
    // 1. Calculate relative height in the cloud volume (0.0 to 1.0)
    // Ensure p.y is clamped to your layer bounds first if not already handled
    float heightFraction = (p.y - cloudAltitude) / cloudThickness;
    heightFraction = clamp(heightFraction, 0.0, 1.0);

    // 2. Apply Coordinate Stretch (from Step 1)
    vec3 uvw = p * vec3(0.001, 0.000035, 0.001);

    uvw += vec3(sin(time * 0.01), abs(sin(p.x)+cos(p.z))*0.001, cos(time*0.01));


    // 3. Sample Base Noise (Red Channel)
    vec4 noiseData = texture(cloudNoiseLUT, uvw);
    float baseCloud = noiseData.r;

    // 4. Apply The Gradient (Step 2)
    // This cuts the noise into a "Cloud Slice" shape
    baseCloud *= getCloudGradient(heightFraction);

    // 5. Calculate Erosion Details
    // Mix High/Mid freq details
    float detailFBM = noiseData.g * 0.625 + noiseData.b * 0.25 + noiseData.a * 0.125;

    // 6. Height-Dependent Erosion Strength
    // Bottom (0.0): Less erosion (Keep it flat/heavy)
    // Top (1.0): More erosion (Make it wispy)
    float erosionStrength = mix(0.2, 0.9, (sin(time*0.01)*0.5+0.5)*noiseData.a*heightFraction);

    // 7. Erode
    float finalDensity = remap(baseCloud, detailFBM * erosionStrength, 1.0, 0.0, 1.0);

    return clamp(finalDensity - cloudDensity, 0.0, 1.0);
}
*/

float sampleCloudDensity(vec3 p) {
	// 1. BOUNDS CHECK
	// Normalized height within the cloud layer (0.0 at bottom, 1.0 at top)
	float heightFraction = (p.y - cloudAltitude) / cloudThickness;
	if (heightFraction < 0.0 || heightFraction > 1.0)
		return 0.0;

	// 2. COORDINATE SCALING
	// We scale the base shape (large) and details (small) differently.
	// 'p' is likely in meters. 0.0005 = 2km repeat pattern (large shapes).
	vec3 p_large = p * 0.005;
	// Anisotropy: Sample Y slower to make them taller
	p_large.y *= 0.5;

	// Animate the lookup to simulate wind
	p_large += vec3(time * 0.05, 0.0, 0.003 * p.z * sin(time * 0.05));

	// 3. SAMPLE VOLUME TEXTURE
	vec4 noise = texture(cloudNoiseLUT, p_large);

	// 4. BASE CLOUD SHAPE (Low Frequency)
	// The Red channel defines the "blob" size.
	float baseCloud = noise.r;

	// Apply Vertical Gradient (The "Anvil" Profile)
	// Bottom is flat/hard, Top is billowy/soft.
	float bottomShape = smoothstep(0.0, 0.15, heightFraction);
	float topShape = 1.0 - smoothstep(0.7, 1.0, heightFraction);
	// Multiply to trim the top and bottom of the cylinder
	baseCloud *= bottomShape * topShape;

	// 5. DETAIL EROSION (High Frequency)
	// Combine Green/Blue channels for fine detail
	float detailNoise = noise.g * 0.625 + noise.b * 0.25 + noise.a * 0.125;

	// Scale erosion strength by height.
	// Bottom of cloud (0.0) is solid (0.0 erosion).
	// Top of cloud (1.0) is wispy (1.0 erosion).
	float erosionStrength = mix(0.1, 0.9, heightFraction);

	// THE "ANTI-MARSHMALLOW" REMAP
	// We use the detail noise to raise the "floor" of the base cloud.
	// If detailNoise is high, we remove more density.
	float density = remap(baseCloud, detailNoise * erosionStrength, 1.0, 0.0, 1.0);

	// 6. COVERAGE & SHARPENING
	// Subtract a threshold to break up the "infinite field" into clusters
	float coverage = 0.6; // Tweak this: 0.2 = overcast, 0.6 = scattered clouds
	density = density - coverage;

	// CRITICAL: Sharpen the result.
	// Multiply by a high number so the transition from 0.0 to 1.0 happens fast.
	// This turns "fog" into "solid cumulus".
	density = clamp(density * 5.0, 0.0, 1.0);

	return density * cloudDensity;
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
