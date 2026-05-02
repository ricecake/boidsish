// Auto-exposure SSBO
layout(std430, binding = [[AUTO_EXPOSURE_BINDING]]) buffer AutoExposure {
	float adaptedLuminance;
	float targetLuminance;
	float minExposure;
	float maxExposure;
	int   useAutoExposure;

	uint  totalLogLuma;
	uint  totalPixelCount;
	uint  workgroupCounter;
};
