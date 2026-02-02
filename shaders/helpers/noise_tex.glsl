#ifndef NOISE_TEX_GLSL
#define NOISE_TEX_GLSL

// Noise texture uniform
// We set the binding point from C++ using glUniform1i
uniform sampler3D u_noiseTex;

/**
 * Sample the noise texture.
 * p: world position or any 3D coordinate.
 * returns: RGBA noise (R=Simplex, G=FBM, B=Warped FBM, A=Detail)
 */
vec4 getNoise(vec3 p) {
    return texture(u_noiseTex, p);
}

float getSimplex(vec3 p) {
    return texture(u_noiseTex, p).r;
}

float getFbm(vec3 p) {
    return texture(u_noiseTex, p).g;
}

float getWarpedFbm(vec3 p) {
    return texture(u_noiseTex, p).b;
}

float getDetail(vec3 p) {
    return texture(u_noiseTex, p).a;
}

#endif // NOISE_TEX_GLSL
