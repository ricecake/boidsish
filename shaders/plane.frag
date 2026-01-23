#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec4 ReflectionClipSpacePos;

#include "helpers/lighting_simple.glsl"

uniform sampler2D reflectionTexture;
uniform sampler3D noiseTexture;
uniform bool      useReflection;

vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(vec4 r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}

float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < 4; i++) {
        value += amplitude * (texture(noiseTexture, p * frequency).r * 2.0 - 1.0);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
	// discard;
	// --- Reflection sampling ---
	vec3 reflectionColor = vec3(0.0);
	if (useReflection) {
		vec2 texCoords = ReflectionClipSpacePos.xy / ReflectionClipSpacePos.w / 2.0 + 0.5;
		reflectionColor = texture(reflectionTexture, texCoords).rgb;
	}

	// --- Grid logic ---
	float grid_spacing = 1.0;
	vec2  coord = WorldPos.xz / grid_spacing;
	vec2  f = fwidth(coord);

	vec2  grid_minor = abs(fract(coord - 0.5) - 0.5) / f;
	float line_minor = min(grid_minor.x, grid_minor.y);
	float C_minor = 1.0 - min(line_minor, 1.0);

	vec2  grid_major = abs(fract(coord / 5.0 - 0.5) - 0.5) / f;
	float line_major = min(grid_major.x, grid_major.y);
	float C_major = 1.0 - min(line_major, 1.0);

	float intensity = max(C_minor, C_major * 1.5) * 0.6;
	vec3  grid_color = vec3(0.0, 0.8, 0.8) * intensity;

	// --- Plane lighting ---
	vec3 norm = normalize(Normal);
	vec3 surfaceColor = vec3(0.05, 0.05, 0.08);
	vec3 lighting = apply_lighting(WorldPos, norm, surfaceColor, 0.05, 0.8);

	// --- Combine colors ---
	float reflection_strength = 0.8;
	vec3  final_color = mix(lighting * surfaceColor, reflectionColor, reflection_strength) + grid_color;

	// --- Distance Fade ---
	vec3  warp = vec3(fbm(WorldPos / 50 + time * 0.08));
	float nebula_noise = fbm(WorldPos / 50 + warp * 0.8);

	float dist = length(WorldPos.xz - viewPos.xz);
	float fade_start = 580.0;
	float fade_end = 600.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + nebula_noise * 50);

	vec4 outColor = vec4(final_color, fade);
	FragColor = mix(vec4(0.7, 0.1, 0.7, fade) * length(outColor), outColor, step(1, fade));
}
