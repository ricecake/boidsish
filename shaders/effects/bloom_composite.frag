#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
uniform sampler2D tileExposureMap;
uniform sampler2D fusedExposureMap;
uniform sampler2D depthTexture;

uniform float     intensity;
uniform float     minIntensity;
uniform float     maxIntensity;

uniform bool toneMappingEnabled = false;
uniform int  toneMapMode = 2;

uniform float uchimuraP;
uniform float uchimuraA;
uniform float uchimuraM;
uniform float uchimuraL;
uniform float uchimuraC;
uniform float uchimuraB;

// New uniforms for depth linearization and bilateral weighting
uniform float nearPlane; // Set from application (e.g., 0.1)
uniform float farPlane;  // Set from application (e.g., 1000.0)
uniform float depthSharpness = 0.50; // Controls edge harshness (start around 1.0 - 5.0)


#include "helpers/tonemapping.glsl"
#include "types/autoexposure.glsl"
// #include "lygia/color/vibrance.glsl"

float linearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

float getBilateralAdaptedLuminance(vec2 uv, float fragDepthLinear) {
    vec2 tileMapRes = textureSize(tileExposureMap, 0);
    vec2 tilePos = uv * tileMapRes - 0.5;
    vec2 baseTile = floor(tilePos);
    vec2 f = fract(tilePos);

    float totalWeight = 0.0;
    float totalLuma = 0.0;

    // Fetch the 4 neighboring tiles
    for (int y = 0; y <= 1; y++) {
        for (int x = 0; x <= 1; x++) {
            vec2 offset = vec2(float(x), float(y));
            vec2 tileCoord = clamp(baseTile + offset, vec2(0.0), tileMapRes - 1.0);

            float tileLuma = texelFetch(tileExposureMap, ivec2(tileCoord), 0).r;

            // Approximate the depth of this tile by sampling the center of its footprint
            vec2 tileUV = (tileCoord + 0.5) / tileMapRes;
            float tileDepthRaw = texture(depthTexture, tileUV).r;
            float tileDepthLinear = linearizeDepth(tileDepthRaw);

            // Bilinear spatial weight
			f = smoothstep(0, 1, f);
            float spatialWeight = mix(1.0 - f.x, f.x, offset.x) * mix(1.0 - f.y, f.y, offset.y);

            // Depth edge-aware weight
            float depthWeight = exp(-abs(fragDepthLinear - tileDepthLinear) * depthSharpness);

            float weight = spatialWeight * depthWeight;

            totalLuma += tileLuma * weight;
            totalWeight += weight;
        }
    }

    // Fallback if weights reach zero to avoid division by zero
    return totalWeight > 0.0001 ? (totalLuma / totalWeight) : texelFetch(tileExposureMap, ivec2(baseTile), 0).r;
}

// Planckian locus approximation for temperature to RGB
vec3 tempToRgb(float temp) {
    temp /= 100.0;
    vec3 rgb;

    if (temp <= 66.0) {
        rgb.r = 255.0;
        rgb.g = clamp(99.4708025861 * log(temp) - 161.1195681661, 0.0, 255.0);
        if (temp <= 19.0) {
            rgb.b = 0.0;
        } else {
            rgb.b = clamp(138.5177312231 * log(temp - 10.0) - 305.0447927307, 0.0, 255.0);
        }
    } else {
        rgb.r = clamp(329.698727446 * pow(temp - 60.0, -0.1332047592), 0.0, 255.0);
        rgb.g = clamp(288.1221695283 * pow(temp - 60.0, -0.0755148492), 0.0, 255.0);
        rgb.b = 255.0;
    }

    return rgb / 255.0;
}

void main() {
	vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
	vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;

	// Add bloom to HDR scene color.
	vec3 result = sceneColor + bloomColor * intensity;

	// 1. White Balance
	vec3 whiteGain = 1.0 / max(tempToRgb(whiteTemp), 0.0001);
	// Apply tint (green/magenta)
	whiteGain.g *= (1.0 - whiteTint * 0.1);
	whiteGain.rb *= (1.0 + whiteTint * 0.05);

	// Normalize whiteGain to preserve luminance
	whiteGain /= max(dot(whiteGain, vec3(0.2126, 0.7152, 0.0722)), 0.0001);
	result *= whiteGain;

	if (useAutoExposure != 0) {
		float gExposure = targetLuminance / max(adaptedLuminance, 0.0001);
		gExposure = clamp(gExposure, minExposure, maxExposure);

		if (ltmEnabled != 0) {
			// Guided Upsampling
			// See https://bartwronski.com/2019/09/22/local-linear-models-guided-filter/
			vec2 tileMapRes = textureSize(fusedExposureMap, 0);
			vec2 invTileMapRes = 1.0 / tileMapRes;

			float momentX = 0.0;
			float momentY = 0.0;
			float momentX2 = 0.0;
			float momentXY = 0.0;
			float ws = 0.0;

			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					vec2 uv = TexCoords + vec2(dx, dy) * invTileMapRes;
					float x = textureLod(tileExposureMap, uv, 0).g; // Midtone synthetic luma
					float y = textureLod(fusedExposureMap, uv, 0).r; // Fused luma
					float w = exp(-0.5 * float(dx*dx + dy*dy) / (0.7*0.7));

					momentX += x * w;
					momentY += y * w;
					momentX2 += x * x * w;
					momentXY += x * y * w;
					ws += w;
				}
			}

			momentX /= ws;
			momentY /= ws;
			momentX2 /= ws;
			momentXY /= ws;

			float A = (momentXY - momentX * momentY) / (max(momentX2 - momentX * momentX, 0.0) + 0.00001);
			float B = momentY - A * momentX;

			// Current pixel's tonemapped luminance (proxy)
			vec3 proxyColor = aces(result);
			float luma = sqrt(dot(proxyColor, vec3(0.2126, 0.7152, 0.0722)));

			float fusedLuma = A * luma + B;
			float localMultiplier = max(fusedLuma, 0.0) / (luma + 0.00001);

			// Prevents artifacts in very dark areas
			float lerpToUnityThreshold = 0.01;
			localMultiplier = luma > lerpToUnityThreshold ? localMultiplier :
				mix(1.0, localMultiplier, smoothstep(0.0, lerpToUnityThreshold, luma));

			result *= localMultiplier;
		} else {
			float rawDepth = texture(depthTexture, TexCoords).r;
			float fragDepthLinear = linearizeDepth(rawDepth);
			float localAdaptedLuma = getBilateralAdaptedLuminance(TexCoords, fragDepthLinear);

			float lExposure = targetLuminance / max(localAdaptedLuma, 0.0001);
			lExposure = clamp(lExposure, minExposure, maxExposure);
			result *= mix(lExposure, gExposure, abs(gExposure - lExposure)/(gExposure+lExposure));
		}
	}

	// 3. ASC CDL Color Grading
	// Color = pow(max(0, Color * Slope + Offset), Power)
	result = pow(max(result * cdlSlope.rgb + cdlOffset.rgb, 0.0), cdlPower.rgb);

	// Saturation
	float luma = dot(result, vec3(0.2126, 0.7152, 0.0722));
	result = luma + cdlSaturation * (result - luma);

	// 4. Tonemapping
	if (toneMappingEnabled) {
		if (toneMapMode == 5) {
			if (autoTuneEnabled != 0) {
				result = uchimura(result, autoUchimuraP, autoUchimuraA, autoUchimuraM, autoUchimuraL, autoUchimuraC, autoUchimuraB);
			} else {
				result = uchimura(result, uchimuraP, uchimuraA, uchimuraM, uchimuraL, uchimuraC, uchimuraB);
			}
		} else {
			result = applyTonemapping(result, toneMapMode);
		}
	}

	// const vec3 a = vec3(0.5, 0.5, 0.5);
	// const vec3 b = vec3(0.5, 0.5, 0.5);
	// const vec3 c = vec3(0.8, 0.8, 0.8);
	// const vec3 d = vec3(0.0, 0.33, 0.67); // Shifts for R, G, B

		// vec3 a = vec3(0.5, 0.5, 0.5);
		// vec3 b = vec3(0.5, 0.5, 0.5);
		// vec3 c = vec3(2.0, 1.0, 0.0);
		// vec3 d = vec3(5.0, 0.2, 0.25); // Shifts for R, G, B

		// vec3 a = vec3(0.5, 0.5, 0.5);
		// vec3 b = vec3(0.5, 0.5, 0.5);
		// vec3 c = vec3(2.0, 1.0, 0.0);
		// vec3 d = vec3(0.50, 0.2, 0.25); // Shifts for R, G, B


	// float value = distance(result, vec3(0.5));

	// result = (a + b * cos(6.28318 * (c * (value) + d)));

	// result = vibrance(result, 0.70);

	FragColor = vec4(result, 1.0);
}
