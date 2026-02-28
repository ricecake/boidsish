#ifndef ARTISTIC_EFFECTS_FRAG
#define ARTISTIC_EFFECTS_FRAG

#include "helpers/noise.glsl"
#include "visual_effects.glsl"

vec3 applyBlackAndWhite(vec3 color) {
	if (black_and_white_enabled == 1) {
		float average = (color.r + color.g + color.b) / 3.0;
		return vec3(average, average, average);
	}
	return color;
}

vec3 applyColorShift(vec3 color, vec3 fragPos, float time) {
	if (color_shift_enabled == 1) {
		float shift_magnitude = 0.2;
		float shift_speed = 5.0;
		vec3  pos_based_shift;
		pos_based_shift.r = sin(fragPos.x * shift_speed) * shift_magnitude;
		pos_based_shift.g = sin(fragPos.y * shift_speed) * shift_magnitude;
		pos_based_shift.b = sin(fragPos.z * shift_speed) * shift_magnitude;
		color += pos_based_shift;

		int posterize_levels = 5;
		color.r = floor(color.r * posterize_levels) / posterize_levels;
		color.g = floor(color.g * posterize_levels) / posterize_levels;
		color.b = floor(color.b * posterize_levels) / posterize_levels;
	}
	return color;
}

vec3 applyNegative(vec3 color) {
	if (negative_enabled == 1) {
		return vec3(1.0 - color.r, 1.0 - color.g, 1.0 - color.b);
	}
	return color;
}

vec3 applyShimmery(vec3 color, float time) {
	if (shimmery_enabled == 1) {
		float shimmer = (sin(time * 10.0) + 1.0) / 2.0;
		return color + vec3(shimmer * 0.2, shimmer * 0.2, shimmer * 0.2);
	}
	return color;
}

vec3 applyGlitched(vec3 color, vec3 fragPos, float time) {
	if (glitched_enabled == 1) {
		float glitchStrength = 0.2;
		float bandHeight = 0.1;
		float speed = 20.0;
		float bands = floor(fragPos.y / bandHeight);
		if (mod(bands, 2.0) == 0.0) {
			if (sin(time * speed + bands) > 0.5) {
				discard;
			}
		}
		float glitch = (sin(time * 50.0) + 1.0) / 2.0;
		if (glitch > 0.9) {
			return color.bgr;
		}
	}
	return color;
}

vec3 applyWireframe(vec3 color, vec3 barycentric) {
	if (wireframe_enabled == 1) {
		float edge_factor = min(barycentric.x, min(barycentric.y, barycentric.z));
		if (edge_factor < 0.05) {
			return vec3(0.0, 0.0, 0.0);
		}
		discard;
	}
	return color;
}

vec4 applyCloak(vec4 outColor, vec3 fragPos, float time, float rim, int perObjectEnabled) {
	if (cloak_enabled == 1 || perObjectEnabled == 1) {
		vec3  warp_offset = vec3(fbm(fragPos + time * 0.05));
		float nebula_noise = fbm(fragPos + warp_offset * 0.5);

		return rim * (1.0 - outColor) + mix(vec4(outColor.rgb, 0.0), rim + outColor, nebula_noise);
	}
	return outColor;
}

vec4 applyArtisticEffects(vec4 color, vec3 fragPos, vec3 barycentric, float time, float rim, int perObjectCloak) {
	vec3 rgb = color.rgb;
	rgb = applyBlackAndWhite(rgb);
	rgb = applyColorShift(rgb, fragPos, time);
	rgb = applyNegative(rgb);
	rgb = applyShimmery(rgb, time);
	rgb = applyGlitched(rgb, fragPos, time);
	rgb = applyWireframe(rgb, barycentric);

	vec4 result = vec4(rgb, color.a);
	result = applyCloak(result, fragPos, time, rim, perObjectCloak);

	return result;
}

#endif
