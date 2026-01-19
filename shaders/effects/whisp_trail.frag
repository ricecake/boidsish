#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D trailTexture; // Previous frame's trail

uniform float time;
uniform vec2  resolution;

void main() {
	// --- 1. Process the previous trail ---
	vec2 offset = 1.0 / resolution;

	// Simple 4-tap blur
	vec4 prevTrailColor = vec4(0.0);
	prevTrailColor += texture(trailTexture, TexCoords + vec2(offset.x, 0.0));
	prevTrailColor += texture(trailTexture, TexCoords + vec2(-offset.x, 0.0));
	prevTrailColor += texture(trailTexture, TexCoords + vec2(0.0, offset.y));
	prevTrailColor += texture(trailTexture, TexCoords + vec2(0.0, -offset.y));
	prevTrailColor *= 0.25;

	// Fade and contract the trail
	prevTrailColor *= 0.95; // Fade out

	// --- 2. Add the current scene ---
	vec4 sceneColor = texture(sceneTexture, TexCoords);

	// Blend the new scene on top of the faded trail.
	// Use max to keep the brightest parts, creating a persistent glow.
	FragColor = max(sceneColor, prevTrailColor);
}
