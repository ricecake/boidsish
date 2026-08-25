#ifndef MICROFACET_GLINTING_GLSL
#define MICROFACET_GLINTING_GLSL

#include "constants.glsl"

/*
Code for "Evaluating and Sampling Glinty NDFs in Constant Time"
	(Pauli Kemppinen, Loïs Paulin, Théo Thonat, Jean-Marc Thiery, Jaakko Lehtinen and Tamy Boubekeur)
	presented at Siggraph Asia 2025. See https://perso.telecom-paristech.fr/boubek/papers/Glinty/
*/

struct GlintProperties {
	float intensity;       // Overall glint multiplier/strength (0.0 to disable)
	float density;         // Glint micro-facet spatial density (e.g. 1000.0 - 10000.0)
	float micro_roughness; // Microfacet roughness / sharpness (e.g. 0.001 - 0.05)
	float filter_size;     // Pixel filter size (e.g. 0.5 - 1.2)
	float scale;           // Spatial frequency / coordinate scaling factor
};

// Default parameters if not provided
const float DEFAULT_MICROFACET_ROUGHNESS = 0.01; // good values are roughly in [0.001, 0.1]
const float DEFAULT_PIXEL_FILTER_SIZE = 0.7; // good values are roughly in [0.5, 1.2]

GlintProperties sanitize_glint_properties(GlintProperties g, float macro_roughness) {
	if (g.intensity <= 0.0) {
		g.intensity = 0.0;
		return g;
	}
	if (g.density <= 0.0) {
		g.density = mix(1000.0, 5000.0, clamp(macro_roughness, 0.0, 1.0));
	}
	if (g.micro_roughness <= 0.0) {
		g.micro_roughness = mix(0.01, 0.005, clamp(macro_roughness, 0.0, 1.0));
	}
	if (g.filter_size <= 0.0) {
		g.filter_size = DEFAULT_PIXEL_FILTER_SIZE;
	}
	if (g.scale <= 0.0) {
		g.scale = 1.0;
	}
	return g;
}

GlintProperties mixGlintProperties(GlintProperties a, GlintProperties b, float t) {
	GlintProperties res;
	res.intensity       = mix(a.intensity, b.intensity, t);
	res.density         = mix(a.density, b.density, t);
	res.micro_roughness = mix(a.micro_roughness, b.micro_roughness, t);
	res.filter_size     = mix(a.filter_size, b.filter_size, t);
	res.scale           = mix(a.scale, b.scale, t);
	return res;
}

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

/**
 * Evaluates the glinty NDF.
 * @param h Half-vector in local tangent space.
 * @param alpha Roughness of the macro-surface.
 * @param glint_alpha Roughness of the microfacets (glint sharpess).
 * @param uv UV coordinates.
 * @param uv_J UV Jacobian (derivatives).
 * @param glint_density Glint density parameter.
 * @param filter_size Pixel filter size.
 */
float glint_ndf(vec3 h, float alpha, float glint_alpha, vec2 uv, mat2 uv_J, float glint_density, float filter_size) {

    float N = glint_density;
    float res = sqrt(N);
    vec2 x_s = uv;
    vec3 x_a_and_d = glint_ndf_to_disk_ggx(h, alpha);
    vec2 x_a = x_a_and_d.xy;
    float d = x_a_and_d.z;

    float lambda = glint_QueryLod(res * uv_J, filter_size);

    float D_filter = .0;

    for(float m = .0; m<2.; m += 1.) {
        float l = floor(lambda) + m;

        float w_lambda = 1. - abs(lambda - l);
        float res_s = res * pow(2., -l);
        float res_a = pow(2., l);

        mat2 uv_J2 = filter_size * uv_J;
        mat2 sigma_s = uv_J2 * transpose(uv_J2);

        mat2 sigma_a = d * pow(glint_alpha, 2.) * mat2(1., .0, .0, 1.);

        vec2 base_i_a = clamp(round(x_a * res_a), 1., res_a-1.);
        for(int j_a = 0; j_a < 4; ++j_a) {
            vec2 i_a = base_i_a + vec2(ivec2(j_a, j_a/2)%2)-.5;

            vec2 base_i_s = round(x_s * res_s);
            for(int j_s = 0; j_s < 4; ++j_s) {
                vec2 i_s = base_i_s + vec2(ivec2(j_s, j_s/2)%2)-.5;

                vec2 g_s = (i_s + glint_Rand2D(i_s, i_a, l, 1u) - .5) / res_s;
                vec2 g_a = (i_a + glint_Rand2D(i_s, i_a, l, 2u) - .5) / res_a;

                float r = glint_Rand1D(i_s, i_a, l, 4u);
                float roulette = smoothstep(max(.0, r-.1), min(1.0, r+.1), w_lambda);

                D_filter += roulette * glint_normal(sigma_a, x_a - g_a) * glint_normal(sigma_s, x_s - g_s) / N;
            }
        }
       D_filter += w_lambda * glint_compensation(x_a, sigma_a, res_a);
    }

    return D_filter * d / PI;
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
*/


float calculate_glint_ndf(vec3 H, vec3 N, float roughness, float metallic, vec2 uv, mat2 uv_J, GlintProperties glint);

float calculate_glint_ndf(vec3 H, vec3 N, float roughness, float metallic, vec2 uv, GlintProperties glint) {
	mat2 uv_J = mat2(0.01, 0.0, 0.0, 0.01);
#ifdef FRAGMENT_SHADER
	uv_J = mat2(dFdx(uv), dFdy(uv));
#endif
	return calculate_glint_ndf(H, N, roughness, metallic, uv, uv_J, glint);
}

float calculate_glint_ndf(vec3 H, vec3 N, float roughness, float metallic, vec2 uv) {
	GlintProperties default_glint;
	default_glint.intensity = 1.0;
	default_glint.density = 0.0;
	default_glint.micro_roughness = 0.0;
	default_glint.filter_size = 0.0;
	default_glint.scale = 1.0;
	return calculate_glint_ndf(H, N, roughness, metallic, uv, default_glint);
}

float calculate_glint_ndf(vec3 H, vec3 N, float roughness, float metallic, vec2 uv, mat2 uv_J) {
	GlintProperties default_glint;
	default_glint.intensity = 1.0;
	default_glint.density = 0.0;
	default_glint.micro_roughness = 0.0;
	default_glint.filter_size = 0.0;
	default_glint.scale = 1.0;
	return calculate_glint_ndf(H, N, roughness, metallic, uv, uv_J, default_glint);
}

float calculate_glint_ndf(vec3 H, vec3 N, float roughness, float metallic, vec2 uv, mat2 uv_J, GlintProperties glint) {
	glint = sanitize_glint_properties(glint, roughness);
	if (glint.intensity <= 0.0) return 0.0;

	float alpha = max(roughness * roughness, 0.0001);

	mat3 invTBN = transpose(get_tangent_basis(N));
	vec3 h_local = invTBN * H;

	vec2 scaled_uv = uv * glint.scale;
	mat2 scaled_uv_J = uv_J * glint.scale;

	float ndf = glint_ndf(h_local, alpha, glint.micro_roughness, scaled_uv, scaled_uv_J, glint.density, glint.filter_size);

	return ndf * 100.0 * glint.intensity;
}

float calculate_snow_glints(vec3 frag_pos, vec3 N, vec3 V, vec3 L, float roughness, float snowFactor) {
	if (snowFactor <= 0.001) return 0.0;

	vec3 H = normalize(V + L);
	float NdotL = max(0.0, dot(N, L));
	if (NdotL <= 0.0) return 0.0;

	// Scale position for fine snow crystal spacing
	vec3 P = frag_pos * 35.0;

	// Distance fade to avoid sub-pixel noise aliasing far away
#ifdef FRAGMENT_SHADER
	float pixelSize = length(fwidth(frag_pos));
	float distFade = 1.0 - smoothstep(0.02, 0.5, pixelSize);
#else
	float distFade = 1.0;
#endif

	if (distFade <= 0.001) return 0.0;

	// Multi-tap jittered grid for dense, organic micro-crystal glints
	vec3 baseCell = floor(P);
	float totalGlint = 0.0;

	for (int x = -1; x <= 1; ++x) {
		for (int z = -1; z <= 1; ++z) {
			vec3 cell = baseCell + vec3(float(x), 0.0, float(z));

			uvec2 ucell = uvec2(ivec2(cell.xz));
			uvec2 h = glint_shuffle(ucell + uvec2(uint(cell.y * 13.0), 0u));
			vec3 randVal = vec3(vec2(h) * pow(0.5, 32.0), float(h.x ^ h.y) * pow(0.5, 32.0));

			vec3 crystalN = normalize(N + (randVal - 0.5) * 0.7);

			float HdotC = max(0.0, dot(H, crystalN));
			float microSpec = pow(HdotC, 350.0);
			float sparkGate = smoothstep(0.85, 0.98, randVal.x);

			totalGlint += microSpec * sparkGate;
		}
	}

	return totalGlint * snowFactor * distFade * 150.0 * NdotL;
}

#endif // MICROFACET_GLINTING_GLSL
