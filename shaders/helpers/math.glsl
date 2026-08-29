
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
	const vec3 magic = vec3( 0.06711056f, 0.00583715f, 52.9829189f );
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

/**
 * Calculates the world size (width/height) of a screen pixel at a given distance from the camera.
 *
 * @param dist Distance from the camera in world units.
 * @param proj11 Value of projection matrix at index [1][1] (1.0 / tan(fovY / 2)).
 * @param screenHeight Height of the viewport in pixels.
 * @return World size of one screen pixel at distance dist.
 */
float calculateDistanceToPixelWorldSize(float dist, float proj11, float screenHeight) {
	return (2.0 * max(0.0, dist)) / max(0.0001, proj11 * screenHeight);
}

/**
 * Calculates the area of a pixel (or footprint) from its world dimension.
 *
 * @param pixelWorldSize Size of the pixel in world units.
 * @return Area of the pixel in square world units.
 */
float calculatePixelArea(float pixelWorldSize) {
	return pixelWorldSize * pixelWorldSize;
}

/**
 * Calculates texture mipmap LOD level from pixel footprint area relative to single texture texel area.
 *
 * @param pixelArea Area of a screen pixel in world space (square units).
 * @param texelArea World space area covered by a single texel at LOD 0 (square units).
 * @return Texture mipmap LOD level (unclamped).
 */
float calculateAreaToTextureLod(float pixelArea, float texelArea) {
	return 0.5 * log2(max(1e-8, pixelArea) / max(1e-8, texelArea));
}

/**
 * Calculates texture mipmap LOD level directly from screen pixel world size and single texel world size.
 *
 * @param pixelWorldSize World size of a screen pixel.
 * @param texelWorldSize World size of a single texture texel at LOD 0.
 * @return Texture mipmap LOD level (unclamped).
 */
float calculatePixelWorldSizeToTextureLod(float pixelWorldSize, float texelWorldSize) {
	return log2(max(1e-8, pixelWorldSize) / max(1e-8, texelWorldSize));
}

// 0,6,8,14,