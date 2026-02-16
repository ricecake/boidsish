#version 420 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 vColor;

uniform int   style; // 0-4: Colors, 5: RAINBOW, 6: INVISIBLE
uniform float time;
uniform float radius;

void main() {
	if (style == 6)
		discard;

	// uv in [-1, 1]
	vec2  uv = TexCoords * 2.0 - 1.0;
	float dist = length(uv);
	float angle = atan(uv.y, uv.x);

	vec3 color = vColor;
	if (style == 5) {
		color = 0.5 + 0.5 * cos(time * 3.0 + dist * 5.0 + angle + vec3(0, 2, 4));
	}

	// Core ring at dist = 0.5
	float ringR = 0.5;
	float ringWidth = 0.03;
	float ringMask = smoothstep(ringR - ringWidth, ringR, dist) * (1.0 - smoothstep(ringR, ringR + ringWidth, dist));

	// Inner Halo: from 0.2 to 0.47
	float haloStart = 0.2;
	float haloEnd = ringR - ringWidth;
	float haloMask = smoothstep(haloStart, haloEnd, dist) * (1.0 - step(haloEnd, dist));

	// Outer Aura: from 0.53 to 1.0
	float auraStart = ringR + ringWidth;
	float auraEnd = 1.0;
	float auraMask = step(auraStart, dist) * (1.0 - smoothstep(auraStart, auraEnd, dist));

	// Glow and Pulsing
	float pulse = sin(time * 4.0 - dist * 10.0) * 0.5 + 0.5;
	float slowPulse = sin(time * 2.0) * 0.5 + 0.5;

	// Directional cue: radial scrolling towards center
	float radialScroll = sin(dist * 20.0 + time * 10.0);
	float directionMask = smoothstep(0.0, 0.2, radialScroll);

	// Directional cue: Chevron pattern
	float chevronCount = 8.0;
	float chevronSpeed = 2.0;
	float chevronPattern = sin(angle * chevronCount + time * chevronSpeed + dist * 5.0);
	float chevronMask = smoothstep(0.5, 0.6, chevronPattern);

	// Combine masks
	float alpha = ringMask * 0.9;
	alpha += haloMask * 0.5 * (0.7 + 0.3 * directionMask);
	alpha += auraMask * 0.4 * (0.7 + 0.3 * slowPulse);

	// Add chevrons to the halo
	alpha += haloMask * chevronMask * 0.2;

	// Add bright highlights on the ring
	float highlights = step(0.8, sin(angle * 10.0 + time * 2.0));
	vec3  finalColor = color;
	if (style != 2) { // Not BLACK
		finalColor += highlights * 0.4;
	} else {
		finalColor += highlights * 0.15;
	}

	// Fade out at the very edges of the quad
	alpha *= (1.0 - smoothstep(0.9, 1.0, dist));

	if (alpha < 0.01)
		discard;

	FragColor = vec4(finalColor * (1.0 + ringMask), alpha);
}
