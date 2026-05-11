#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D      sceneTexture;
uniform sampler2DArray bloomBlur;
uniform sampler2DArray ltmFused;
uniform sampler2DArray ltmExpMip;
uniform sampler2D      depthTexture;
uniform vec2           ltmRes;
uniform float          intensity;
uniform float          minIntensity;
uniform float          maxIntensity;

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

vec3 tempToRgb(float temp) {
    temp /= 100.0;
    vec3 rgb;
    if (temp <= 66.0) {
        rgb.r = 255.0;
        rgb.g = clamp(99.4708025861 * log(temp) - 161.1195681661, 0.0, 255.0);
        if (temp <= 19.0) rgb.b = 0.0;
        else rgb.b = clamp(138.5177312231 * log(temp - 10.0) - 305.0447927307, 0.0, 255.0);
    } else {
        rgb.r = clamp(329.698727446 * pow(temp - 60.0, -0.1332047592), 0.0, 255.0);
        rgb.g = clamp(288.1221695283 * pow(temp - 60.0, -0.0755148492), 0.0, 255.0);
        rgb.b = 255.0;
    }
    return rgb / 255.0;
}

vec3 applyLayerLTM(vec3 color, int layer, LayerData ld, vec2 uv) {
	if (ld.ltmEnabled == 0) return color;
	float exposure = ld.targetLuminance / max(ld.avgLuma, 0.0001);
	vec3 currentExposure = color * exposure;
	vec3 currentAces = aces(currentExposure);
	float guidanceLuma = sqrt(max(dot(currentAces, vec3(0.2126, 0.7152, 0.0722)), 0.0));

	float momentX = 0.0; float momentY = 0.0; float momentX2 = 0.0; float momentXY = 0.0; float weightSum = 0.0;
	vec2 texelSize = 1.0 / ltmRes;
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			vec2 offset = vec2(dx, dy) * texelSize;
			float x = texture(ltmExpMip, vec3(uv + offset, layer)).y; // mid-exposure
			float y = texture(ltmFused, vec3(uv + offset, layer)).r;  // fused
			float w = exp(-0.5 * float(dx*dx + dy*dy) / (0.7 * 0.7));
			momentX += x * w; momentY += y * w; momentX2 += x * x * w; momentXY += x * y * w; weightSum += w;
		}
	}
	momentX /= weightSum; momentY /= weightSum; momentX2 /= weightSum; momentXY /= weightSum;
	float A = (momentXY - momentX * momentY) / (max(momentX2 - momentX * momentX, 0.0) + 0.00001);
	float B = momentY - A * momentX;
	float localFusedLuma = max(A * guidanceLuma + B, 0.0);
	float finalMultiplier = localFusedLuma / max(guidanceLuma, 0.0001);
	float lerpToUnityThreshold = 0.007;
	if (guidanceLuma < lerpToUnityThreshold) {
		float t = guidanceLuma / lerpToUnityThreshold;
		finalMultiplier = mix(1.0, finalMultiplier, t * t);
	}
	return color * finalMultiplier;
}

vec3 applyLayerGrading(vec3 result, LayerData ld) {
	vec3 whiteGain = 1.0 / max(tempToRgb(ld.whiteTemp), 0.0001);
	whiteGain.g *= (1.0 - ld.whiteTint * 0.1);
	whiteGain.rb *= (1.0 + ld.whiteTint * 0.05);
	whiteGain /= max(dot(whiteGain, vec3(0.2126, 0.7152, 0.0722)), 0.0001);
	result *= whiteGain;

	if (ld.useAutoExposure != 0) {
		float exposure = ld.targetLuminance / max(ld.adaptedLuminance, 0.0001);
		exposure = clamp(exposure, ld.minExposure, ld.maxExposure);
		result *= exposure;
	}

	result = pow(max(result * ld.cdlSlope.rgb + ld.cdlOffset.rgb, 0.0), ld.cdlPower.rgb);
	float luma = dot(result, vec3(0.2126, 0.7152, 0.0722));
	result = luma + ld.cdlSaturation * (result - luma);
	return result;
}

void main() {
	vec4 sceneSample = texture(sceneTexture, TexCoords);
	vec3 sceneColor = sceneSample.rgb;
	float sceneMask = sceneSample.a;
	float isScene = step(0.5, sceneMask);

	vec3 bloomScene = texture(bloomBlur, vec3(TexCoords, 0)).rgb;
	vec3 bloomSky = texture(bloomBlur, vec3(TexCoords, 1)).rgb;

	// Process layers independently
	vec3 groundResult = sceneColor * isScene + bloomScene * intensity;
	vec3 skyResult = sceneColor * (1.0 - isScene) + bloomSky * intensity;

	// LTM
	groundResult = applyLayerLTM(groundResult, 0, scene, TexCoords);
	skyResult = applyLayerLTM(skyResult, 1, sky, TexCoords);

	// Grading & Exposure
	groundResult = applyLayerGrading(groundResult, scene);
	skyResult = applyLayerGrading(skyResult, sky);

	// Tonemapping
	if (sceneToneMappingEnabled) {
		if (sceneToneMapMode == 5) {
			if (scene.autoTuneEnabled != 0) groundResult = uchimura(groundResult, scene.autoUchimuraP, scene.autoUchimuraA, scene.autoUchimuraM, scene.autoUchimuraL, scene.autoUchimuraC, scene.autoUchimuraB);
			else groundResult = uchimura(groundResult, sceneUchimuraP, sceneUchimuraA, sceneUchimuraM, sceneUchimuraL, sceneUchimuraC, sceneUchimuraB);
		} else groundResult = applyTonemapping(groundResult, sceneToneMapMode);
	}
	if (skyToneMappingEnabled) {
		if (skyToneMapMode == 5) {
			if (sky.autoTuneEnabled != 0) skyResult = uchimura(skyResult, sky.autoUchimuraP, sky.autoUchimuraA, sky.autoUchimuraM, sky.autoUchimuraL, sky.autoUchimuraC, sky.autoUchimuraB);
			else skyResult = uchimura(skyResult, skyUchimuraP, skyUchimuraA, skyUchimuraM, skyUchimuraL, skyUchimuraC, skyUchimuraB);
		} else skyResult = applyTonemapping(skyResult, skyToneMapMode);
	}

	// Final composite based on mask
	FragColor = vec4(mix(skyResult, groundResult, isScene), 1.0);
}
