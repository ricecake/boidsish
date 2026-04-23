#ifndef LBM_COMMON_GLSL
#define LBM_COMMON_GLSL

const int D3Q19_X[19] = int[](0,  1,-1, 0, 0, 0, 0,  1,-1, 1,-1, 1,-1, 1,-1, 0, 0, 0, 0);
const int D3Q19_Y[19] = int[](0,  0, 0, 1,-1, 0, 0,  1, 1,-1,-1, 0, 0, 0, 0, 1,-1, 1,-1);
const int D3Q19_Z[19] = int[](0,  0, 0, 0, 0, 1,-1,  0, 0, 0, 0, 1, 1,-1,-1, 1, 1,-1,-1);

const float D3Q19_W[19] = float[](
    1.0/3.0,                                          // i=0
    1.0/18.0, 1.0/18.0, 1.0/18.0, 1.0/18.0, 1.0/18.0, 1.0/18.0, // i=1-6
    1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0, // i=7-12
    1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0  // i=13-18
);

const int D3Q19_OPPOSITE[19] = int[](0, 2,1, 4,3, 6,5, 10,9, 8,7, 14,13, 12,11, 18,17, 16,15);

layout(std140, binding = 0) uniform LbmParams {
    vec4 u_gravity;    // xyz = gravity, w = dt
    vec4 u_lbmParams;  // x = tau, y = omega, z = amount, w = unused
    ivec4 u_resolution; // xyz = res, w = unused
    vec4 u_worldScale;  // xyz = scale, w = radius
    vec4 u_worldOrigin; // xyz = origin, w = unused
};

float calculate_equilibrium(int i, float rho, vec3 u) {
    vec3 ci = vec3(D3Q19_X[i], D3Q19_Y[i], D3Q19_Z[i]);
    float cu = dot(ci, u);
    float uu = dot(u, u);
    return D3Q19_W[i] * rho * (1.0 + 3.0*cu + 4.5*cu*cu - 1.5*uu);
}

#endif
