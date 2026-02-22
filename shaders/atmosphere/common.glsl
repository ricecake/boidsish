#ifndef ATMOSPHERE_COMMON_GLSL
#define ATMOSPHERE_COMMON_GLSL

#define PI 3.14159265359

// Physical Constants
const float kEarthRadius = 6360.0; // km
const float kAtmosphereHeight = 60.0; // km
const float kTopRadius = kEarthRadius + kAtmosphereHeight;

const vec3 kRayleighScattering = vec3(5.802, 13.558, 33.100) * 1e-3; // km^-1
const float kRayleighScaleHeight = 8.0; // km

const float kMieScattering = 3.996 * 1e-3; // km^-1
const float kMieExtinction = 4.440 * 1e-3; // km^-1
const float kMieScaleHeight = 1.2; // km
const float kMieG = 0.8;

const vec3 kOzoneAbsorption = vec3(0.650, 1.881, 0.085) * 1e-3; // km^-1

// Helper functions
bool intersectSphere(vec3 ro, vec3 rd, float radius, out float t0, out float t1) {
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float det = b * b - c;
    if (det < 0.0) return false;
    det = sqrt(det);
    t0 = -b - det;
    t1 = -b + det;
    return true;
}

float getRayleighDensity(float h) {
    return exp(-h / kRayleighScaleHeight);
}

float getMieDensity(float h) {
    return exp(-h / kMieScaleHeight);
}

float getOzoneDensity(float h) {
    return max(0.0, 1.0 - abs(h - 25.0) / 15.0);
}

struct Sampling {
    vec3 rayleigh;
    vec3 mie;
    vec3 extinction;
};

Sampling getAtmosphereProperties(float h) {
    float rd = getRayleighDensity(h);
    float md = getMieDensity(h);
    float od = getOzoneDensity(h);

    Sampling s;
    s.rayleigh = kRayleighScattering * rd;
    s.mie = vec3(kMieScattering * md);
    s.extinction = s.rayleigh + vec3(kMieExtinction * md) + kOzoneAbsorption * od;
    return s;
}

// Phase Functions
float rayleighPhase(float cosTheta) {
    return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

float miePhase(float cosTheta) {
    float g = kMieG;
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

// LUT mapping functions
vec2 transmittanceToUV(float r, float mu) {
    float H = sqrt(kTopRadius * kTopRadius - kEarthRadius * kEarthRadius);
    float rho = sqrt(max(0.0, r * r - kEarthRadius * kEarthRadius));
    float d = mu < 0.0 ? (rho - sqrt(rho * rho - r * r + kEarthRadius * kEarthRadius)) : (rho + sqrt(rho * rho - r * r + kTopRadius * kTopRadius));
    float d_min = kTopRadius - r;
    float d_max = rho + H;
    float u = (d - d_min) / (d_max - d_min);
    float v = (r - kEarthRadius) / kAtmosphereHeight;
    return vec2(u, v);
}

void UVToTransmittance(vec2 uv, out float r, out float mu) {
    float x_mu = uv.x;
    float x_r = uv.y;
    r = kEarthRadius + x_r * kAtmosphereHeight;
    float rho = sqrt(max(0.0, r * r - kEarthRadius * kEarthRadius));
    float H = sqrt(kTopRadius * kTopRadius - kEarthRadius * kEarthRadius);
    float d_min = kTopRadius - r;
    float d_max = rho + H;
    float d = d_min + x_mu * (d_max - d_min);
    mu = d == 0.0 ? 1.0 : (H * H - rho * rho - d * d) / (2.0 * r * d);
    mu = clamp(mu, -1.0, 1.0);
}

#endif
