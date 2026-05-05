// Auto-exposure SSBO
layout(std430, binding = [[AUTO_EXPOSURE_BINDING]]) buffer AutoExposure {
	float adaptedLuminance;
	float targetLuminance;
	float minExposure;
	float maxExposure;

	int   useAutoExposure;
	float centerWeightTightness;
	vec2  focusPoint;

	float histogramLowCutoff;
	float histogramHighCutoff;
	uint  workgroupCounter;
	uint  _pad;

	uint  histogram[256];
};
