#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
uniform sampler2D ltmFused;
uniform sampler2D ltmExpMip;
uniform vec2      ltmRes;
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

#include "helpers/tonemapping.glsl"
#include "types/autoexposure.glsl"
// #include "lygia/color/vibrance.glsl"

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

	// Guided Upsampling for LTM
	if (ltmEnabled != 0) {
		float exposure = targetLuminance / max(avgLuma, 0.0001);
		vec3 currentExposure = result * exposure;
		vec3 currentAces = aces(currentExposure);
		float guidanceLuma = sqrt(max(dot(currentAces, vec3(0.2126, 0.7152, 0.0722)), 0.0));

		// Sample 3x3 neighborhood from low-res textures for guided filter
		float momentX = 0.0;
		float momentY = 0.0;
		float momentX2 = 0.0;
		float momentXY = 0.0;
		float weightSum = 0.0;

		vec2 texelSize = 1.0 / ltmRes;

		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				vec2 offset = vec2(dx, dy) * texelSize;
				float x = texture(ltmExpMip, TexCoords + offset).y; // mid-exposure lightness
				float y = texture(ltmFused, TexCoords + offset).r;  // fused lightness

				float w = exp(-0.5 * float(dx*dx + dy*dy) / (0.7 * 0.7));
				momentX += x * w;
				momentY += y * w;
				momentX2 += x * x * w;
				momentXY += x * y * w;
				weightSum += w;
			}
		}

		momentX /= weightSum;
		momentY /= weightSum;
		momentX2 /= weightSum;
		momentXY /= weightSum;

		float A = (momentXY - momentX * momentY) / (max(momentX2 - momentX * momentX, 0.0) + 0.00001);
		float B = momentY - A * momentX;

		float localFusedLuma = max(A * guidanceLuma + B, 0.0);
		float finalMultiplier = localFusedLuma / max(guidanceLuma, 0.0001);

		// Prevent artifacts in very dark areas
		float lerpToUnityThreshold = 0.007;
		if (guidanceLuma < lerpToUnityThreshold) {
			float t = guidanceLuma / lerpToUnityThreshold;
			finalMultiplier = mix(1.0, finalMultiplier, t * t);
		}

		result *= finalMultiplier;
	}

	// 1. White Balance
	vec3 whiteGain = 1.0 / max(tempToRgb(whiteTemp), 0.0001);
	// Apply tint (green/magenta)
	whiteGain.g *= (1.0 - whiteTint * 0.1);
	whiteGain.rb *= (1.0 + whiteTint * 0.05);

	// Normalize whiteGain to preserve luminance
	whiteGain /= max(dot(whiteGain, vec3(0.2126, 0.7152, 0.0722)), 0.0001);
	result *= whiteGain;

	// 2. Exposure
	if (useAutoExposure != 0) {
		float exposure = targetLuminance / max(adaptedLuminance, 0.0001);
		exposure = clamp(exposure, minExposure, maxExposure);
		result *= exposure;
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
