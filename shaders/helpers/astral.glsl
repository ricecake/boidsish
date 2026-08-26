#ifndef HELPERS_ASTRAL_GLSL
#define HELPERS_ASTRAL_GLSL

#include "noise.glsl"
#include "lygia/color/palette.glsl"
#include "../visual_effects.glsl"

float hash13(in vec3 pos) {
    pos  = fract(pos * vec4(.1031, .1030, .0973, .1099).xyz);
    pos += dot(pos, pos.zyx + 31.32);
    return fract((pos.x + pos.y) * pos.z);
}

float fbm_astral(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		v += a * snoise3d(p);
		p *= 2.0;
		a *= 0.5;
	}
	return v;
}

vec3 snoise3_astral(vec3 x) {
    float s  = snoise3d(x);
    float s1 = snoise3d(vec3(x.y - 19.1, x.z + 33.4, x.x + 47.2));
    float s2 = snoise3d(vec3(x.z + 74.2, x.x - 124.5, x.y + 99.4));
    return vec3(s, s1, s2);
}

vec3 curl_astral(vec3 p) {
    const float e = .1;
    vec3 dx = vec3(e, 0.0, 0.0);
    vec3 dy = vec3(0.0, e, 0.0);
    vec3 dz = vec3(0.0, 0.0, e);

    vec3 p_x0 = snoise3_astral(p - dx);
    vec3 p_x1 = snoise3_astral(p + dx);
    vec3 p_y0 = snoise3_astral(p - dy);
    vec3 p_y1 = snoise3_astral(p + dy);
    vec3 p_z0 = snoise3_astral(p - dz);
    vec3 p_z1 = snoise3_astral(p + dz);

    float x = p_y1.z - p_y0.z - p_z1.y + p_z0.y;
    float y = p_z1.x - p_z0.x - p_x1.z + p_x0.z;
    float z = p_x1.y - p_x0.y - p_y1.x + p_y0.x;

    return vec3(x, y, z) * (1.0 / (2.0 * e));
}

vec3 computeNebula(vec3 dir, float time) {
    if (nebula_enabled == 0 || nebula_intensity <= 0.0001) {
        return vec3(0.0);
    }

    vec3 p = dir * 4.0;
    vec3 warp_offset = vec3(fbm_astral(p + time * 0.05));
    float raw_noise = fbm_astral(p + warp_offset * 0.5);

    // Threshold noise to control net coloration coverage while maintaining physical continuity
    float nebula_noise = smoothstep(nebula_threshold, 1.0, raw_noise);
    if (nebula_noise <= 0.0001) {
        return vec3(0.0);
    }

    vec3 nebula_color = palette(nebula_noise, vec3(0.5,0.5,0.5), vec3(0.5,0.5,0.5), vec3(1.0,1.0,1.0), vec3(0.0,0.33,0.67));
    // Physically grounded nebula radiance (~0.0003 W/m^2/sr)
    return 0.0003 * nebula_intensity * snoise3d(warp_offset) * nebula_color * nebula_noise;
}

vec3 computeStars(vec3 dir, float time) {
    dir = normalize(dir);

    vec3 warp = vec3(2,2,2);
    float scale = 128.0;
    vec3 id = floor(dir * scale);
    vec3 local_uv = fract(dir * scale);

    vec3 star_pos = hash33(id);
    vec3 center = vec3(0.5) + (star_pos - 0.5) * 0.5;

    vec3 global_star_dir = normalize((id + center) / scale);
    float starDensity = snoise3d(global_star_dir * warp);
    float starMask = smoothstep(-1.0, 1.50, starDensity);

    float checkHash = hash13(id);
    float starExists = step(checkHash, starMask);

    float dist = length(local_uv - center);
    float brightness = (10.0 + sin(checkHash + time/(1.0+max(0.01, dir.y)) )) * starExists;
    float radius = 0.0125 * brightness;

    float visualGlow = snoise3d(dir * warp);
    float starIntensity = 1.0 - smoothstep(radius * 0.5, radius, dist);

    vec3 starColor1 = palette(hash13(id+dir), vec3(0.17, 0.47, 0.92), vec3(0.55, 0.4, 0.4), vec3(1., 1.7, 1.), vec3(0.5, 0.35, 1));
    vec3 starColor2 = palette(hash13(id+dir), vec3(0.48, 0.47, 0.89), vec3(0.14, 0, 0.63), vec3(1.5, 0., 0.6), vec3(0, 0, 0));

    // Physically grounded starlight radiance (~0.0001 ambient starlight glow, ~0.002 point star peaks)
    vec3 backgroundGlow = 0.0001 * pow(smoothstep(-0.40, 0.9, visualGlow), 2.0) * starColor2;
    vec3 pointStars = 0.002 * starIntensity * mix(starColor1, starColor2, checkHash);

    return backgroundGlow + pointStars;
}

#endif // HELPERS_ASTRAL_GLSL
