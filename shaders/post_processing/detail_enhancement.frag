#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform vec2 uResolution;

// Controls
uniform float uStrength; // master effect strength

// Detail Boost Curve
uniform float uStrengthExponent;
uniform float uStrengthScale;
uniform float uStrengthBias;

// Guiding Strength (Epsilon) Curve
uniform float uEpsilonBase;
uniform float uEpsilonExponent;
uniform float uEpsilonScale;
uniform float uEpsilonBias;

void main() {
	vec3 originalColor = textureLod(screenTexture, TexCoords, 0.0).rgb;
	vec3 enhancedColor = originalColor;

	for (int i = 1; i <= 4; i++) {
		float i_float = float(i);

		// Compute level parameters using the curves
		float boost = uStrengthScale * pow(i_float, uStrengthExponent) + uStrengthBias;
		float eps = uEpsilonBase * (uEpsilonScale * pow(i_float, uEpsilonExponent) + uEpsilonBias);
		eps = max(eps, 1e-6); // safeguard against division by zero

		vec3 fineColor = textureLod(screenTexture, TexCoords, float(i - 1)).rgb;
		vec3 coarseColor = textureLod(screenTexture, TexCoords, float(i)).rgb;
		vec3 laplacian = fineColor - coarseColor;

		// Local Guided Filter over 3x3 window on the finer level (i-1)
		float mean_G = 0.0;
		float mean_G2 = 0.0;
		float weightSum = 0.0;
		vec2 texelSize = vec2(pow(2.0, float(i - 1))) / uResolution;

		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				vec2 offset = vec2(dx, dy) * texelSize;
				vec3 c = textureLod(screenTexture, TexCoords + offset, float(i - 1)).rgb;
				float G = dot(c, vec3(0.2126, 0.7152, 0.0722)); // Guidance is luminance

				// Simple Gaussian weight
				float w = exp(-0.5 * float(dx*dx + dy*dy));
				mean_G += G * w;
				mean_G2 += G * G * w;
				weightSum += w;
			}
		}

		mean_G /= max(weightSum, 0.0001);
		mean_G2 /= max(weightSum, 0.0001);

		float var_G = max(mean_G2 - mean_G * mean_G, 0.0);

		// guided weight a = var_G / (var_G + epsilon)
		float a = var_G / (var_G + eps);

		// Detail is scaled by (1 - a) to suppress near edges/halos
		vec3 detail = (1.0 - a) * laplacian;

		// Apply detail boost scaled by master strength
		enhancedColor += uStrength * (boost - 1.0) * detail;
	}

	FragColor = vec4(enhancedColor, 1.0);
}
