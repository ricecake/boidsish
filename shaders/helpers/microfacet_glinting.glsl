#ifndef MICROFACET_GLINTING_GLSL
#define MICROFACET_GLINTING_GLSL

#include "constants.glsl"

/*
Code for "Evaluating and Sampling Glinty NDFs in Constant Time"
	(Pauli Kemppinen, Loïs Paulin, Théo Thonat, Jean-Marc Thiery, Jaakko Lehtinen and Tamy Boubekeur)
	presented at Siggraph Asia 2025. See https://perso.telecom-paristech.fr/boubek/papers/Glinty/
*/

// Default parameters if not provided
const float DEFAULT_MICROFACET_ROUGHNESS = 0.01; // good values are roughly in [0.001, 0.1]
const float DEFAULT_PIXEL_FILTER_SIZE = 0.7; // good values are roughly in [0.5, 1.2]

vec2 glint_lambert(vec3 v) {
    return v.xy / sqrt(1. + v.z);
}

vec3 glint_ndf_to_disk_ggx(vec3 v, float alpha) {
    vec3 hemi = vec3(v.xy / alpha, v.z);
    float denom = dot(hemi, hemi);
    vec2 v_disk = glint_lambert(normalize(hemi))*.5 + .5;
    float jacobian_determinant = 1. / (alpha * alpha * denom * denom);
    return vec3(v_disk, jacobian_determinant);
}

mat2 glint_inv_quadratic(mat2 M) {
	float D = determinant(M);
	D = sign(D) * max(abs(D), 1e-9);
	float A = dot(M[0] / D, M[0] / D);
	float B = -dot(M[0] / D, M[1] / D);
	float C = dot(M[1] / D, M[1] / D);
	return mat2(C, B, B, A);
}

mat2 glint_uv_ellipsoid(mat2 uv_J) {
	mat2 Q = glint_inv_quadratic(transpose(uv_J));
	float tr = .5 * (Q[0][0] + Q[1][1]);
	float  D = sqrt(max(.0, tr * tr - determinant(Q)));
	float l1 = tr - D;
	float l2 = tr + D;
	vec2 v1 = vec2(l1 - Q[1][1], Q[0][1]);
	vec2 v2 = vec2(Q[1][0], l2 - Q[0][0]);
	vec2 n = 1.f/sqrt(vec2(l1, l2));
	return mat2(normalize(v1)*n.x, normalize(v2)*n.y);
}

float glint_QueryLod(mat2 uv_J, float filter_size) {
	float s0 = length(uv_J[0]), s1 = length(uv_J[1]);
	return log2(max(max(s0, s1), 1e-6) * filter_size) + pow(2., filter_size);
}

uvec2 glint_shuffle(uvec2 v) {
    v = v * 1664525u + 1013904223u;
	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;

	v = v ^ (v>>uvec2(16u));

	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;
	v = v ^ (v>>uvec2(16u));
    return v;
}

vec2 glint_rand(uvec2 v) {
	return vec2(glint_shuffle(v)) * pow(.5, 32.);
}

float glint_normal(mat2 cov, vec2 x) {
	float det = determinant(cov);
	if (det < 1e-12)
		return 0.0;
	return exp(-.5 * dot(x, inverse(cov) * x)) / (sqrt(det) * 2. * PI);
}

vec2 glint_Rand2D(vec2 x, vec2 y, float l, uint i) {
    uvec2 ux = floatBitsToUint(x), uy = floatBitsToUint(y);
    uint ul = floatBitsToUint(l);
    return glint_rand((ux>>16u|ux<<16u) ^ uy ^ ul ^ (i*0x124u));
}

float glint_Rand1D(vec2 x, vec2 y, float l, uint i) {
    return glint_Rand2D(x, y, l, i).x;
}

// Bürmann series, see https://en.wikipedia.org/wiki/Error_function
float glint_erf(float x) {
    float e = exp(-x*x);
    return sign(x) * 2. * sqrt( (1. - e) / PI ) * ( sqrt(PI) * .5 + 31./200. * e - 341./8000. * e * e );
}

float glint_cdf(float x, float mu, float sigma) {
	return .5 + .5 * glint_erf((x-mu)/(sigma*sqrt(2.)));
}

float glint_integrate_interval(float x, float size, float mu, float stdev, float lower_limit, float upper_limit) {
	return glint_cdf(min(x+size, upper_limit), mu, stdev) - glint_cdf(max(x-size, lower_limit), mu, stdev);
}

float glint_integrate_box(vec2 x, vec2 size, vec2 mu, mat2 sigma, vec2 lower_limit, vec2 upper_limit) {
	return
		glint_integrate_interval(x.x, size.x, mu.x, sqrt(sigma[0][0]), lower_limit.x, upper_limit.x) *
		glint_integrate_interval(x.y, size.y, mu.y, sqrt(sigma[1][1]), lower_limit.y, upper_limit.y);
}


float glint_compensation(vec2 x_a, mat2 sigma_a, float res_a) {
    float containing = glint_integrate_box(vec2(.5), vec2(.5), x_a, sigma_a, vec2(.0), vec2(1.));
    float explicitly_evaluated = glint_integrate_box(round(x_a*res_a)/res_a, vec2(1./res_a), x_a, sigma_a, vec2(.0), vec2(1.));
    return containing - explicitly_evaluated;
}

struct GlintLevelContext {
    float l;
    float w_lambda;
    float res_s;
    float res_a;
    vec2 base_i_s;
};

struct GlintContext {
    float N;
    float res;
    float lambda;
    mat2 sigma_s;
    GlintLevelContext levels[2];
};

/**
 * Prepares the glint context for a fragment.
 * This precalculates values that are independent of the light direction.
 */
GlintContext prepare_glint_context(vec2 uv, mat2 uv_J, float glint_density, float filter_size) {
    GlintContext ctx;
    ctx.N = glint_density;
    ctx.res = sqrt(ctx.N);
    ctx.lambda = glint_QueryLod(ctx.res * uv_J, filter_size);

    mat2 uv_J2 = filter_size * uv_J;
    ctx.sigma_s = uv_J2 * transpose(uv_J2);

    for(int i = 0; i < 2; ++i) {
        float l = floor(ctx.lambda) + float(i);
        ctx.levels[i].l = l;
        ctx.levels[i].w_lambda = 1. - abs(ctx.lambda - l);
        ctx.levels[i].res_s = ctx.res * pow(2., -l);
        ctx.levels[i].res_a = pow(2., l);
        ctx.levels[i].base_i_s = round(uv * ctx.levels[i].res_s);
    }
    return ctx;
}

/**
 * Evaluates the glinty NDF using a precalculated context.
 * @param h Half-vector in local tangent space.
 * @param alpha Roughness of the macro-surface.
 * @param glint_alpha Roughness of the microfacets (glint sharpness).
 * @param uv UV coordinates.
 * @param ctx Precalculated glint context.
 */
float evaluate_glint_ndf(vec3 h, float alpha, float glint_alpha, vec2 uv, GlintContext ctx) {
    vec3 x_a_and_d = glint_ndf_to_disk_ggx(h, alpha);
    vec2 x_a = x_a_and_d.xy;
    float d = x_a_and_d.z;

    float D_filter = .0;

    for(int i = 0; i < 2; ++i) {
        GlintLevelContext lctx = ctx.levels[i];
        float l = lctx.l;
        float w_lambda = lctx.w_lambda;
        float res_s = lctx.res_s;
        float res_a = lctx.res_a;

        mat2 sigma_a = d * pow(glint_alpha, 2.) * mat2(1., .0, .0, 1.);

        vec2 base_i_a = clamp(round(x_a * res_a), 1., res_a-1.);
        for(int j_a = 0; j_a < 4; ++j_a) {
            vec2 i_a = base_i_a + vec2(ivec2(j_a, j_a/2)%2)-.5;

            vec2 base_i_s = lctx.base_i_s;
            for(int j_s = 0; j_s < 4; ++j_s) {
                vec2 i_s = base_i_s + vec2(ivec2(j_s, j_s/2)%2)-.5;

                vec2 g_s = (i_s + glint_Rand2D(i_s, i_a, l, 1u) - .5) / res_s;
                vec2 g_a = (i_a + glint_Rand2D(i_s, i_a, l, 2u) - .5) / res_a;

                float r = glint_Rand1D(i_s, i_a, l, 4u);
                float roulette = smoothstep(max(.0, r-.1), min(1.0, r+.1), w_lambda);

                D_filter += roulette * glint_normal(sigma_a, x_a - g_a) * glint_normal(ctx.sigma_s, uv - g_s) / ctx.N;
            }
        }
       D_filter += w_lambda * glint_compensation(x_a, sigma_a, res_a);
    }

    return D_filter * d / PI;
}

/**
 * Evaluates the glinty NDF (legacy combined version).
 */
float glint_ndf(vec3 h, float alpha, float glint_alpha, vec2 uv, mat2 uv_J, float glint_density, float filter_size) {
    GlintContext ctx = prepare_glint_context(uv, uv_J, glint_density, filter_size);
    return evaluate_glint_ndf(h, alpha, glint_alpha, uv, ctx);
}

/**
 * Generate a tangent space basis from a normal.
 */
mat3 get_tangent_basis(vec3 N) {
	vec3 helper = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
	vec3 T = normalize(cross(helper, N));
	vec3 B = cross(N, T);
	return mat3(T, B, N);
}

/**
 * Calculate the glinty Normal Distribution Function.
 * Derives parameters from material properties if not provided.
 */

/*
vec2 uv = frag_pos.xz;
mat2 uv_J = mat2(0.01, 0.0, 0.0, 0.01);
#ifdef FRAGMENT_SHADER
uv_J = mat2(dFdx(uv), dFdy(uv));
#endif

float glintNDF = calculate_glint_ndf(H, N, roughness, metallic, uv, uv_J);
// Additive blending with intensity control to prevent negative extrapolation
NDF = NDF + glintNDF * glintIntensity;



+       mat2 uv_J = mat2(dFdx(TexCoords), dFdy(TexCoords));
+       vec3 lighting = apply_lighting_pbr(
+               FragPos,
+               perturbedNorm,
+               albedo,
+               roughness,
+               metallic,
+               1.0,
+               TexCoords,
+               uv_J,
+               false,
+               0.0,
+               0.0
+       ).rgb;
*/


float calculate_glint_ndf(vec3 H, vec3 N, float roughness, float metallic, vec2 uv, mat2 uv_J) {
	float alpha = max(roughness * roughness, 0.0001);

	// Derive glint parameters based on material properties
	// Snow-like (rough dielectric): many fine glints
	// Metal-like: few intense (sharp) glints
	float g_density, g_alpha;
	if (metallic > 0.1) {
		// Metals: few intense glints
		g_density = mix(1000.0, 200.0, clamp(metallic, 0.0, 1.0));
		g_alpha = mix(0.01, 0.002, clamp(metallic, 0.0, 1.0));
	} else {
		// Dielectrics: more glints as it gets rougher (like snow)
		g_density = mix(500.0, 4000.0, clamp(roughness, 0.0, 1.0));
		g_alpha = mix(0.02, 0.01, clamp(roughness, 0.0, 1.0));
	}

	mat3 invTBN = transpose(get_tangent_basis(N));
	vec3 h_local = invTBN * H;

	// Use the Siggraph Asia 2025 glinting algorithm
	return glint_ndf(h_local, alpha, g_alpha, uv, uv_J, g_density, DEFAULT_PIXEL_FILTER_SIZE);
}

#endif // MICROFACET_GLINTING_GLSL
