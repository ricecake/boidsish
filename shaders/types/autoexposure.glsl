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
	uint  _pad1;

	// Statistics
	float minLuma;
	float maxLuma;
	float avgLuma;
	float stdDevLuma;

	// EMAs
	float emaMinLuma;
	float emaMaxLuma;
	float emaAvgLuma;
	float emaStdDevLuma;

	// Auto-tune settings
	int   autoTuneEnabled;
	float minContrast;
	float maxContrast;
	float targetBrightness;

	// Auto-calculated Uchimura parameters
	float autoUchimuraP;
	float autoUchimuraA;
	float autoUchimuraM;
	float autoUchimuraL;
	float autoUchimuraC;
	float autoUchimuraB;
	float _pad2;
	float _pad3;

	// ASC CDL (vec4 for alignment)
	vec4  cdlSlope;
	vec4  cdlOffset;
	vec4  cdlPower;
	float cdlSaturation;

	// White Balance
	float whiteTemp;
	float whiteTint;
	float _pad4;

	// Local Tone Mapping (Exposure Fusion)
	int   ltmEnabled;
	float ltmEvSpread;
	float ltmTarget;
	float ltmSigma;

	float ltmWeightContrast;
	float ltmWeightSaturation;
	float ltmWeightExposedness;
	float ltmBoostLocalContrast;

	uint  histogram[256];
};
