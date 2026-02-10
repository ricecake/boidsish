#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D currentTexture;
uniform sampler2D historyTexture;
uniform sampler2D motionVectorTexture;
uniform bool      firstFrame;
uniform bool      blitOnly;

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

	if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
		FragColor = current;
		return;
	}

	vec4 history = texture(historyTexture, prevUV);

	// 2. Neighborhood clamping to prevent ghosting
	vec2 texelSize = 1.0 / textureSize(currentTexture, 0);
	vec4 minColor = current;
	vec4 maxColor = current;

	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {
			if (x == 0 && y == 0)
				continue;
			vec4 neighbor = texture(currentTexture, TexCoords + vec2(x, y) * texelSize);
			minColor = min(minColor, neighbor);
			maxColor = max(maxColor, neighbor);
		}
	}

	// Add some padding to the range to reduce flickering
	float padding = 0.1;
	minColor = max(vec4(0.0), minColor - padding);
	maxColor = min(vec4(10.0), maxColor + padding);

	history = clamp(history, minColor, maxColor);

	// 3. Accumulate
	// Dynamic alpha based on motion?
	float motionLength = length(motion);
	float alpha = mix(0.95, 0.7, clamp(motionLength * 100.0, 0.0, 1.0));

	FragColor = mix(current, history, alpha);
}
