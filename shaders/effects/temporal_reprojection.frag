#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D currentTexture;
uniform sampler2D historyTexture;
uniform sampler2D motionVectorTexture;
uniform bool      firstFrame;
uniform bool      blitOnly;

// Parameters
uniform float feedbackMin = 0.85;
uniform float feedbackMax = 0.99;
uniform float varianceGamma = 1.2;

void main() {
	if (blitOnly) {
		FragColor = texture(historyTexture, TexCoords);
		return;
	}

	vec4 current = texture(currentTexture, TexCoords);
	if (firstFrame) {
		FragColor = current;
		return;
	}

	// 1. Reproject
	vec2 motion = texture(motionVectorTexture, TexCoords).rg;
	vec2 prevUV = TexCoords - motion;

	// Check if previous UV is off-screen
	if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
		FragColor = current;
		return;
	}

	vec4 history = texture(historyTexture, prevUV);

	// 2. Variance Clipping to prevent ghosting
	vec2  texelSize = 1.0 / textureSize(currentTexture, 0);
	vec4  m1 = vec4(0.0);
	vec4  m2 = vec4(0.0);
	int   count = 0;

	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {
			vec4 neighbor = texture(currentTexture, TexCoords + vec2(x, y) * texelSize);
			m1 += neighbor;
			m2 += neighbor * neighbor;
			count++;
		}
	}

	vec4 mean = m1 / float(count);
	vec4 stddev = sqrt(max(vec4(0.0), (m2 / float(count)) - (mean * mean)));

	vec4 minColor = mean - varianceGamma * stddev;
	vec4 maxColor = mean + varianceGamma * stddev;

	history = clamp(history, minColor, maxColor);

	// 3. Accumulate
	float motionLength = length(motion);
	// Increase feedback (alpha) when motion is low for more stability
	// Decrease feedback when motion is high to reduce ghosting
	float alpha = mix(feedbackMax, feedbackMin, clamp(motionLength * 100.0, 0.0, 1.0));

	FragColor = mix(current, history, alpha);
}
