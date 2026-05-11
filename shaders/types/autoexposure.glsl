struct LayerData {
	float adaptedLuminance;
	float targetLuminance;
	float minExposure;
	float maxExposure;

	int   useAutoExposure;
	float centerWeightTightness;
	vec2  focusPoint;

	float histogramLowCutoff;
	float histogramHighCutoff;
	float speedUp;
	float speedDown;

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
	float _pad_ld3;
	float _pad_ld4;

	// ASC CDL (vec4 for alignment)
	vec4  cdlSlope;
	vec4  cdlOffset;
	vec4  cdlPower;
	float cdlSaturation;

	// White Balance
	float whiteTemp;
	float whiteTint;
	float _pad_ld5;

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

// Auto-exposure SSBO
layout(std430, binding = [[AUTO_EXPOSURE_BINDING]]) buffer AutoExposure {
	uint workgroupCounter;
	uint _pad_ae1;
	uint _pad_ae2;
	uint _pad_ae3;

	LayerData scene;
	LayerData sky;
};
