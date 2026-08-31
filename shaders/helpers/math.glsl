
#include "constants.glsl"

float roundToEvenPlaces(float value, float places) {
	float shift = pow(10.0, places);
	return roundEven(value * shift) / shift;
}

float roundToPlaces(float value, float places) {
	float shift = pow(10.0, places);
	return round(value * shift) / shift;
}

float terraceSmooth(float h, float numSteps, float slopeCoarseness) {
    float stepId = floor(h * numSteps);
    float fractional = fract(h * numSteps);

    // Smooth the transition edge between steps
    // slopeCoarseness: 0.0 = perfectly sharp, 1.0 = completely smooth
    float edge = smoothstep(0.0, slopeCoarseness, fractional);

    return (stepId + edge) / numSteps;
}

// High-quality 32-bit integer hash to generate deterministic pseudo-random seeds
uint hashUint(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

// Hierarchical Base-4 Owen Scramble for a 32-bit Morton Code
uint owenScrambleBase4(uint mortonCode, uint seed) {
    uint scrambled = 0U;
    uint currentSeed = seed;

    // Process 16 pairs of bits (32 bits total for a 2D Morton code)
    // We go from the highest-significance digit to the lowest
    for (int i = 15; i >= 0; i--) {
        // Extract the current base-4 digit (2 bits)
        uint digitShift = uint(i * 2);
        uint digit = (mortonCode >> digitShift) & 3U;

        // Generate a pseudo-random 2-bit permutation based on the structural path history
        // Mixing the current path seed with a hash creates the hierarchical scrambling
        currentSeed = hashUint(currentSeed ^ (digit + uint(i)));
        uint permutation = currentSeed & 3U;

        // Apply the permutation (XOR is standard for Owen scrambling)
        uint scrambledDigit = digit ^ permutation;

        // Reconstruct the scrambled code
        scrambled |= (scrambledDigit << digitShift);

        // Feed the scrambled digit forward to downstream children to preserve hierarchy
        currentSeed ^= scrambledDigit;
    }

    return scrambled;
}

// Spreads 16 bits of a uint out to every other bit (32 bits total)
uint part1by1(uint n) {
    n &= 0x0000ffffu;                  // n = ---- ---- ---- ---- fedc ba98 7654 3210
    n = (n ^ (n <<  8u)) & 0x00ff00ffu; // n = ---- ---- fedc ba98 ---- ---- 7654 3210
    n = (n ^ (n <<  4u)) & 0x0f0f0f0fu; // n = ---- fedc ---- ba98 ---- 7654 ---- 3210
    n = (n ^ (n <<  2u)) & 0x33333333u; // n = --fe --dc --ba --98 --76 --54 --32 --10
    n = (n ^ (n <<  1u)) & 0x55555555u; // n = f e d c b a 9 8 7 6 5 4 3 2 1 0
    return n;
}

// Compacts every other bit of a 32-bit uint back into 16 contiguous bits
uint unpart1by1(uint n) {
    n &= 0x55555555u;                  // n = f e d c b a 9 8 7 6 5 4 3 2 1 0
    n = (n ^ (n >>  1u)) & 0x33333333u; // n = --fe --dc --ba --98 --76 --54 --32 --10
    n = (n ^ (n >>  2u)) & 0x0f0f0f0fu; // n = ---- fedc ---- ba98 ---- 7654 ---- 3210
    n = (n ^ (n >>  4u)) & 0x00ff00ffu; // n = ---- ---- fedc ba98 ---- ---- 7654 3210
    n = (n ^ (n >>  8u)) & 0x0000ffffu; // n = ---- ---- ---- ---- fedc ba98 7654 3210
    return n;
}

// ENCODE: Interleaves two 16-bit values into a 32-bit index
uint encodeMorton2D(uvec2 coords) {
    return part1by1(coords.x) | (part1by1(coords.y) << 1u);
}

// DECODE: Extracts two 16-bit coordinates from a 32-bit Morton code
uvec2 decodeMorton2D(uint code) {
    return uvec2(
        unpart1by1(code),
        unpart1by1(code >> 1u)
    );
}

uint mortonOwenScramble(uvec2 p, uint seed) {
	uint morton = encodeMorton2D(p);
	return owenScrambleBase4(morton, seed);
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

// 0,6,8,14,