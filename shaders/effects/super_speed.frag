#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform float     time;
uniform float     intensity; // 0.0 to 1.0
uniform vec2      center;    // Screen center in 0.0 to 1.0

// Radial blur
vec3 radialBlur(sampler2D tex, vec2 uv, vec2 center, float strength) {
	vec3  color = vec3(0.0);
	int   samples = 12;
	float decay = 0.95;

	vec2  dir = uv - center;
	float dist = length(dir);

	// Sharpen perspective distortion by scaling UVs towards center
	vec2 distortedUV = center + dir * (1.0 - strength * 0.2 * dist);

	for (int i = 0; i < samples; i++) {
		float scale = 1.0 - strength * 0.05 * float(i) / float(samples);
		color += texture(tex, center + (distortedUV - center) * scale).rgb;
	}

	return color / float(samples);
}

void main() {
	vec2 uv = TexCoords;

	// 1. Sonic boom shockwave (distortion-like blur near center)
	float waveDist = length(uv - center);
	float wave = sin(waveDist * 50.0 - time * 20.0) * 0.5 + 0.5;
	float waveMask = smoothstep(0.1, 0.0, abs(waveDist - fract(time * 2.0) * 0.5));
	vec2  distortedUV = uv + normalize(uv - center) * waveMask * 0.02 * intensity;

	// 2. Outward blur + Perspective distortion
	vec3 color = radialBlur(sceneTexture, distortedUV, center, intensity);

	// 3. Add a bit of "sonic boom" highlight
	color += vec3(0.8, 0.9, 1.0) * waveMask * intensity * 0.5;

	FragColor = vec4(color, 1.0);
}
