#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
uniform sampler2D ltmFused;
uniform sampler2D ltmExpMip;
uniform sampler2D depthTexture;
uniform vec2      ltmRes;
uniform float     intensity;
uniform float     minIntensity;
uniform float     maxIntensity;

uniform bool  sceneToneMappingEnabled = false;
uniform int   sceneToneMapMode = 2;
uniform float sceneUchimuraP;
uniform float sceneUchimuraA;
uniform float sceneUchimuraM;
uniform float sceneUchimuraL;
uniform float sceneUchimuraC;
uniform float sceneUchimuraB;

uniform bool  skyToneMappingEnabled = false;
uniform int   skyToneMapMode = 2;
uniform float skyUchimuraP;
uniform float skyUchimuraA;
uniform float skyUchimuraM;
uniform float skyUchimuraL;
uniform float skyUchimuraC;
uniform float skyUchimuraB;

uniform float nearPlane;
uniform float farPlane;

#include "helpers/tonemapping.glsl"
#include "types/autoexposure.glsl"

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

	float depth = texture(depthTexture, TexCoords).r;
	bool isSky = (depth >= 1.0);

	// Select layer data
	// Note: We can't use pointers to SSBO members, so we copy or use if-else.
	// We'll use a temporary LayerData but that's a lot of copying.
	// Instead, we'll access members directly via a macro or helper.

	#define LAYER_VAL(member) (isSky ? sky.member : scene.member)

	// Guided Upsampling for LTM
	if (LAYER_VAL(ltmEnabled) != 0) {
		float exposure = LAYER_VAL(targetLuminance) / max(LAYER_VAL(avgLuma), 0.0001);
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
	vec3 whiteGain = 1.0 / max(tempToRgb(LAYER_VAL(whiteTemp)), 0.0001);
	whiteGain.g *= (1.0 - LAYER_VAL(whiteTint) * 0.1);
	whiteGain.rb *= (1.0 + LAYER_VAL(whiteTint) * 0.05);
	whiteGain /= max(dot(whiteGain, vec3(0.2126, 0.7152, 0.0722)), 0.0001);
	result *= whiteGain;

	// 2. Exposure
	if (LAYER_VAL(useAutoExposure) != 0) {
		float exposure = LAYER_VAL(targetLuminance) / max(LAYER_VAL(adaptedLuminance), 0.0001);
		exposure = clamp(exposure, LAYER_VAL(minExposure), LAYER_VAL(maxExposure));
		result *= exposure;
	}

	// 3. ASC CDL Color Grading
	result = pow(max(result * LAYER_VAL(cdlSlope).rgb + LAYER_VAL(cdlOffset).rgb, 0.0), LAYER_VAL(cdlPower).rgb);
	float luma = dot(result, vec3(0.2126, 0.7152, 0.0722));
	result = luma + LAYER_VAL(cdlSaturation) * (result - luma);

	// 4. Tonemapping
	bool toneMappingEnabled = isSky ? skyToneMappingEnabled : sceneToneMappingEnabled;
	int toneMapMode = isSky ? skyToneMapMode : sceneToneMapMode;

	if (toneMappingEnabled) {
		if (toneMapMode == 5) { // Uchimura
			if (LAYER_VAL(autoTuneEnabled) != 0) {
				result = uchimura(result, LAYER_VAL(autoUchimuraP), LAYER_VAL(autoUchimuraA), LAYER_VAL(autoUchimuraM), LAYER_VAL(autoUchimuraL), LAYER_VAL(autoUchimuraC), LAYER_VAL(autoUchimuraB));
			} else {
				if (isSky) {
					result = uchimura(result, skyUchimuraP, skyUchimuraA, skyUchimuraM, skyUchimuraL, skyUchimuraC, skyUchimuraB);
				} else {
					result = uchimura(result, sceneUchimuraP, sceneUchimuraA, sceneUchimuraM, sceneUchimuraL, sceneUchimuraC, sceneUchimuraB);
				}
			}
		} else {
			result = applyTonemapping(result, toneMapMode);
		}
	}

	FragColor = vec4(result, 1.0);
}
