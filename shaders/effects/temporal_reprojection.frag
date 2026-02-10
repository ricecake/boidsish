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

	vec2 motion = texture(motionVectorTexture, TexCoords).rg;
	vec2 prevUV = TexCoords - motion;

	if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
		FragColor = current;
		return;
	}

	vec4 history = texture(historyTexture, prevUV);

	// Accumulation factor - can be tuned
	float alpha = 0.9;

	FragColor = mix(current, history, alpha);
}
