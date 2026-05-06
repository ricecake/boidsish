#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomBlur;
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

	// 1. Exposure
	if (useAutoExposure != 0) {
		float exposure = targetLuminance / max(adaptedLuminance, 0.0001);
		exposure = clamp(exposure, minExposure, maxExposure);
		result *= exposure;
	}

	// 2. White Balance
	vec3 whiteGain = 1.0 / max(tempToRgb(whiteTemp), 0.0001);
	// Apply tint (green/magenta)
	whiteGain.g *= (1.0 - whiteTint * 0.1);
	whiteGain.rb *= (1.0 + whiteTint * 0.05);

	// Normalize whiteGain to preserve luminance
	whiteGain /= max(dot(whiteGain, vec3(0.2126, 0.7152, 0.0722)), 0.0001);
	result *= whiteGain;

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

	FragColor = vec4(result, 1.0);
}
