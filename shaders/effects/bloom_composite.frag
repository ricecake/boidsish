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

uniform mat4 invView;
uniform mat4 invProjection;

uniform float nearPlane; // Set from application (e.g., 0.1)
uniform float farPlane;  // Set from application (e.g., 1000.0)

uniform float gamma;
uniform bool  uBloomEnabled;


#include "helpers/tonemapping.glsl"
#include "types/autoexposure.glsl"
// #include "lygia/color/vibrance.glsl"
#include "lygia/color/space.glsl"

#include "types/lighting.glsl";

struct CdlEntry {
	vec4  cdlSlope;
	vec4  cdlOffset;
	vec4  cdlPower;
	float cdlSaturation;
	float targetDepth;
	float falloffWidth;
	float falloffRate;
	int   priority;
	int   enabled;
	int   isMain;
	float padding;
};

layout(std430, binding = [[CDL_GRADING_LAYERS_BINDING]]) buffer CdlGradingLayers {
	CdlEntry cdlEntries[];
};

uniform int uNumCdlEntries;

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

float linearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

// Calculates a safe multiplier to prevent sky luminance from blowing out the Uchimura shoulder
float calculateSkyAttenuation(vec3 rawHdrColor, float uchimuraM, float uchimuraL, float rolloffStrength) {
	float luma = dot(rawHdrColor, vec3(0.2126, 0.7152, 0.0722));

	//Define the threshold where Uchimura starts compressing
	float shoulderStart = uchimuraM + uchimuraL;

	//Calculate how far past the linear region this pixel is
	float overdrive = max(0.0, luma - shoulderStart);

	//Apply a rational rolloff: 1 / (1 + x)
	// If overdrive is 0 (below shoulder), multiplier is 1.0.
	// As overdrive increases, multiplier smoothly approaches 0.
	// 'rolloffStrength' (e.g., 0.5 to 2.0) tunes how aggressively you hold onto highlights.
	float multiplier = 1.0 / (1.0 + rolloffStrength * overdrive);

	return multiplier;
}

// Simulates the scotopic rod shift in mesopic lighting conditions
vec3 ApplyPurkinjeShift(vec3 exposedLinearColor, float avgLuminance, vec3 scotopicTint) {
    float scotopicMin = 0.001;
    float photopicMax = 3.0;

    // Calculate logarithmic blend factor
    float logAvg = log(max(avgLuminance, 1e-5));
    float logMin = log(scotopicMin);
    float logMax = log(photopicMax);

    float photopicWeight = clamp((logAvg - logMin) / (logMax - logMin), 0.0, 1.0);
    photopicWeight = smoothstep(0.0, 1.0, photopicWeight);

    // Calculate scene luminance using Rec. 709 luma
    float pixelLuminance = dot(exposedLinearColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 scotopicColor = pixelLuminance * scotopicTint;

    return mix(scotopicColor, exposedLinearColor, photopicWeight);
}

void main() {
	vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
	vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;

	vec3 result = sceneColor;

	float rawDepth = texture(depthTexture, TexCoords).r;
	int isSky = 0;
	if (rawDepth > 0.99999) {
		isSky = 1;
		// vec2 ndc = TexCoords * 2.0 - 1.0;
		// vec4 ray_view = invProjection * vec4(ndc, -1.0, 1.0);
		// ray_view = vec4(ray_view.xy, -1.0, 0.0);
		// vec3 worldDir = normalize((invView * ray_view).xyz);
		// if (worldDir.y > 0.0) {
		// 	isSky = 1;
		// }
	}

	// Guided Upsampling for LTM
	if (layers[isSky].ltmEnabled != 0) {
		float exposure = layers[isSky].targetLuminance / max(layers[isSky].avgLuma, 0.0001);
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

	// 2. Exposure
	float exposure = 1.0;
	if (layers[isSky].useAutoExposure != 0) {
		exposure = layers[isSky].targetLuminance / max(layers[isSky].adaptedLuminance, 0.0001);
		exposure = clamp(exposure, layers[isSky].minExposure, layers[isSky].maxExposure);

		if (isSky == 1) {
			vec2 ndc = TexCoords * 2.0 - 1.0;
			vec4 ray_view = invProjection * vec4(ndc, -1.0, 1.0);
			ray_view = vec4(ray_view.xy, -1.0, 0.0); // Focus on direction
			vec3 worldDir = normalize((invView * ray_view).xyz);

			float attenuation = calculateSkyAttenuation(result * exposure, layers[1].autoUchimuraM, layers[1].autoUchimuraL, 1.20);
			float mask = smoothstep(-3.14 * 0.125, 0.5*1.5707, asin(worldDir.y));
			attenuation = mix(attenuation, 1.0, mask);
			exposure *= attenuation;
		}
	} else {
		exposure = (layers[isSky].iso / 100.0) * (1.0 / (layers[isSky].aperture * layers[isSky].aperture)) * layers[isSky].exposureTime;
	}

	result *= exposure;

	// --- PURKINJE SHIFT ---
	vec3 scotopicTint = vec3(0.15, 0.3, 0.6);
	// vec3 scotopicTint = vec3(2.0, 0.3, 0.6);
	result = ApplyPurkinjeShift(result, layers[isSky].adaptedLuminance, scotopicTint);
	// ----------------------

	if (uBloomEnabled) {
		result += bloomColor * intensity;
	}

	// 1. White Balance
	vec3 whiteGain = 1.0 / max(tempToRgb(layers[isSky].whiteTemp), 0.0001);
	// Apply tint (green/magenta)
	whiteGain.g *= (1.0 - layers[isSky].whiteTint * 0.1);
	whiteGain.rb *= (1.0 + layers[isSky].whiteTint * 0.05);

	// Normalize whiteGain to preserve luminance
	whiteGain /= max(dot(whiteGain, vec3(0.2126, 0.7152, 0.0722)), 0.0001);
	result *= whiteGain;

	// 3. ASC CDL Color Grading (Single or Multi-layer depth based)
	if (isSky == 0) {
		float linearZ = linearizeDepth(rawDepth);
		for (int i = 0; i < uNumCdlEntries; ++i) {
			CdlEntry entry = cdlEntries[i];
			if (entry.enabled == 0) continue;

			float weight = 1.0;
			if (entry.isMain == 0) {
				float dist = abs(linearZ - entry.targetDepth);
				float x = clamp(dist / max(entry.falloffWidth, 0.0001), 0.0, 1.0);
				weight = pow(1.0 - x, entry.falloffRate);
			}

			if (weight > 0.0) {
				vec3 graded = pow(max(result * entry.cdlSlope.rgb + entry.cdlOffset.rgb, 0.0), entry.cdlPower.rgb);
				float layerLuma = dot(graded, vec3(0.2126, 0.7152, 0.0722));
				graded = layerLuma + entry.cdlSaturation * (graded - layerLuma);
				result = mix(result, graded, weight);
			}
		}
	} else {
		// Existing sky color grading
		result = pow(max(result * layers[1].cdlSlope.rgb + layers[1].cdlOffset.rgb, 0.0), layers[1].cdlPower.rgb);
	}

	// 4. Tonemapping
	if (layers[isSky].toneMappingEnabled != 0) {
		int mode = layers[isSky].toneMapMode;
		if (mode == 5) { // Uchimura
			if (layers[isSky].autoTuneEnabled != 0) {
				result = uchimura(result, layers[isSky].autoUchimuraP, layers[isSky].autoUchimuraA, layers[isSky].autoUchimuraM, layers[isSky].autoUchimuraL, layers[isSky].autoUchimuraC, layers[isSky].autoUchimuraB);
			} else {
				result = uchimura(result, layers[isSky].uchimuraP, layers[isSky].uchimuraA, layers[isSky].uchimuraM, layers[isSky].uchimuraL, layers[isSky].uchimuraC, layers[isSky].uchimuraB);
			}
		} else {
			result = applyTonemapping(result, mode);
		}
	}

	// Saturation
	if (isSky == 1) {
		float luma = dot(result, vec3(0.2126, 0.7152, 0.0722));
		result = luma + layers[1].cdlSaturation * (result - luma);
	}

	// 5. Gamma Correction
	result = pow(max(result, 0.0), vec3(1.0 / gamma));

	FragColor = vec4(result, 1.0);
}
