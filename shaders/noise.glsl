#ifndef NOISE_GLSL
#define NOISE_GLSL

// Classic Perlin noise
vec4 permute(vec4 x) {
    return mod(((x*34.0)+1.0)*x, 289.0);
}

vec2 fade(vec2 t) {
    return t*t*t*(t*(t*6.0-15.0)+10.0);
}

float cnoise(vec2 P) {
    vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    Pi = mod(Pi, 256.0);
    vec4 ix = Pi.xzxz;
    vec4 iy = Pi.yyww;
    vec4 fx = Pf.xzxz;
    vec4 fy = Pf.yyww;
    vec2 f = fade(Pf.xy);

    vec4 g00 = permute(Pi + vec4(0.0, 1.0, 289.0, 290.0));
    vec4 g10 = permute(Pi + vec4(1.0, 0.0, 290.0, 289.0));

    vec4 grad00 = fract(g00 / 256.0) * 2.0 - 1.0;
    vec4 grad10 = fract(g10 / 256.0) * 2.0 - 1.0;

    vec4 n00 = grad00 * vec4(fx.x, fy.x, fx.z, fy.z);
    vec4 n10 = grad10 * vec4(fx.y, fy.y, fx.w, fy.w);

    vec2 n_x = mix(vec2(n00.x, n00.y), vec2(n10.x, n10.y), f.x);
    float n_xy = mix(n_x.x, n_x.y, f.y);

    return 2.3 * n_xy;
}


float fbm(vec2 p) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; i++) {
		value += amplitude * cnoise(p);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

#endif // NOISE_GLSL
