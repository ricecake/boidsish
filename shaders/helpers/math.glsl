#ifndef HELPERS_MATH_GLSL
#define HELPERS_MATH_GLSL

#include "constants.glsl"

float roundToEvenPlaces(float value, float places) {
	float shift = pow(10.0, places);
	return roundEven(value * shift) / shift;
}

float roundToPlaces(float value, float places) {
	float shift = pow(10.0, places);
	return round(value * shift) / shift;
}

float henyeyGreenstein(float g, float cosTheta) {
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(max(0.0001, 1.0 + g2 - 2.0 * g * cosTheta), 1.5));
}

vec3 henyeyGreenstein(vec3 g, float cosTheta) {
	if (g.r == g.g && g.g == g.b) {
		float gs = g.r;
		float g2 = gs * gs;
		return vec3((1.0 - g2) / (4.0 * PI * pow(max(0.0001, 1.0 + g2 - 2.0 * gs * cosTheta), 1.5)));
	} else {
		vec3 g2 = g * g;
		vec3 den = vec3(1.0) + g2 - 2.0 * g * cosTheta;
		den = max(vec3(0.0001), den);
		return (vec3(1.0) - g2) / (4.0 * PI * pow(den, vec3(1.5)));
	}
}

float remap(float value, float low1, float high1, float low2, float high2) {
	return low2 + (value - low1) * (high2 - low2) / max(0.0001, (high1 - low1));
}

float bayer4x4StepPhase(ivec2 pixel, int index) {
	const int bayer4x4[16] = int[](
		 0,  8,  2, 10,
		12,  4, 14,  6,
		 3, 11,  1,  9,
		15,  7, 13,  5
	);

	// int timer = ((index) / 2) + 2 * (index%2);
	// ivec2 pixelOffset = ivec2(timer % 4, (timer + 2) %4);

	// ivec2 pixelOffset = ivec2(index % 4, (index + 2) %4);
	// pixel += pixelOffset;

	// if (index % 2 > 0) {
	// 	return float(bayer4x4[((pixel.y & 3) * 4 + (pixel.x & 3)) % 16]) / 16.0;
	// }
	return float(bayer4x4[((pixel.x & 3) * 4 + (pixel.y & 3)) % 16]) / 16.0;

	// float jitter = float(bayer4x4[((pixel.y & 3) * 4 + (pixel.x & 3) + (timer%16)) % 16]) / 16.0;
	// float jitter = float(bayer4x4[((pixel.y & 3) * 4 + (pixel.x & 3) + frameIndex) % 16]) / 16.0;
	// float jitter = float(bayer4x4[((pixel.y & 3) * 4 + (pixel.x & 3)) % 16]) / 16.0;
}

float InterleavedGradientNoise(vec2 uv, int FrameId){
	// uv += float(FrameId)  * (vec2(47, 17) * 0.695f);
	//vec3 magic = vec3( 12.9898, 78.233, 43758.5453123 );
	vec3 magic = vec3( 0.06711056f, 0.00583715f, 52.9829189f );
	float spatialJitter = fract(magic.z * fract(dot(uv, magic.xy)));
	float temporalShift = fract(float(FrameId) * 0.61803398);
	return fract(spatialJitter + temporalShift);
}


//  0, 1, 2, 3,
//  4, 5, 6, 7
//  8, 9,10,11
// 12,13,14,15
//  0, 1, 2, 3,
//  4, 5, 6, 7
//  8, 9,10,11
// 12,13,14,15


// 0, 2, 7, 9, 16
// 0, 5, 8, 13, 2, 5, 10, 13,
// 0, 5, 8, 13,

// 0,6,8,14,

#endif // HELPERS_MATH_GLSL
