#ifndef ATMOSPHERE_COMMON_GLSL
#define ATMOSPHERE_COMMON_GLSL

#include "../helpers/constants.glsl"

// Physical Constants
const float kEarthRadius = 6360.0; // km

#ifndef HAZE_UNIFORMS_DEFINED
#define HAZE_UNIFORMS_DEFINED
uniform float hazeDensity;
uniform float hazeHeight;
uniform vec3  hazeColor;
#endif

#ifndef ATMOSPHERE_HEIGHT_DEFINED
	#define ATMOSPHERE_HEIGHT_DEFINED
uniform float u_atmosphereHeight;
#endif

#ifndef TRANSMITTANCE_LUT_DEFINED
	#define TRANSMITTANCE_LUT_DEFINED
uniform sampler2D u_transmittanceLUT;
#endif

#ifndef WEATHER_TEXTURES_DEFINED
	#define WEATHER_TEXTURES_DEFINED
layout(binding = [[WEATHER_SCALARS_BINDING]]) uniform sampler2D u_weatherScalars;
layout(binding = [[WEATHER_AEROSOLS_BINDING]]) uniform sampler2D u_weatherAerosols;
#endif

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = [[TERRAIN_DATA_BINDING]]) uniform TerrainData {
	ivec4 u_originSize;    // x, z, size, is_bound
	vec4  u_terrainParams; // chunkSize, worldScale
};
#endif

#ifndef WORLD_SCALE_VALUE
	#define WORLD_SCALE_VALUE u_terrainParams.y
#endif

#define kAtmosphereHeight u_atmosphereHeight
#define kTopRadius (kEarthRadius + kAtmosphereHeight)

uniform vec3  u_rayleighScatteringBase;
uniform float u_rayleighScaleHeight;
#define kRayleighScattering u_rayleighScatteringBase
#define kRayleighScaleHeight u_rayleighScaleHeight

uniform float u_mieScatteringBase;
uniform float u_mieExtinctionBase;
uniform float u_mieScaleHeight;
#define kMieScattering u_mieScatteringBase
#define kMieExtinction u_mieExtinctionBase
#define kMieScaleHeight u_mieScaleHeight

uniform float u_rayleighScale;
uniform float u_mieScale;
uniform float u_mieAnisotropy;

uniform vec3 u_ozoneAbsorptionBase;
#define kOzoneAbsorption u_ozoneAbsorptionBase

// Helper functions
bool intersectSphere(vec3 ro, vec3 rd, float radius, out float t0, out float t1) {
	float b = dot(ro, rd);
	float rLen = length(ro);
	float c = (rLen - radius) * (rLen + radius);
	float det = b * b - c;
	if (det < 0.0)
		return false;
	det = sqrt(det);
	t0 = -b - det;
	t1 = -b + det;
	return true;
}

float getRayleighDensity(float h) {
	return exp(-max(0.0, h) / kRayleighScaleHeight);
}

float getMieDensity(float h) {
	return exp(-max(0.0, h) / kMieScaleHeight);
}

float getOzoneDensity(float h) {
	return max(0.0, 1.0 - abs(max(0.0, h) - 25.0) / 15.0);
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

	// Base haze factor from uniforms (applied to LUT generation)
	float groundHaze = hazeDensity * exp(-max(0.0, h) / max(hazeHeight, 1.0));

	Sampling s;
	s.rayleigh = kRayleighScattering * rd * u_rayleighScale;
	s.mie = hazeColor * (kMieScattering * (md + groundHaze) * u_mieScale);
	s.extinction = s.rayleigh + hazeColor * (kMieExtinction * (md + groundHaze) * u_mieScale) + kOzoneAbsorption * od;
	return s;
}

Sampling getAtmospherePropertiesAtPos(vec3 worldPos) {
	float h = worldPos.y / (1000.0 * max(0.0001, WORLD_SCALE_VALUE));
	Sampling s = getAtmosphereProperties(h);

#ifndef ATMOSPHERE_PRECOMPUTE
	// Modulate Mie based on weather
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2 weatherUV = (worldPos.xz / scaledChunkSize - vec2(u_originSize.xy)) / 128.0;
	vec4 scalars = texture(u_weatherScalars, weatherUV);
	vec4 aerosols = texture(u_weatherAerosols, weatherUV);

	float humidity = scalars.y;
	float aerosolConc = aerosols.x + aerosols.y + aerosols.z + aerosols.w;

	// Humidity increases Mie scattering (haze/mist)
	float humidityFactor = 1.0 + humidity * 5.0;
	float aerosolFactor = 1.0 + aerosolConc * 10.0;

	// Enhancement: ensure the "gap" between floor and cloud layer is as dense as the sky in the cloud layer.
	// We boost Mie scattering below the cloud floor to provide continuous atmospheric feel.
	#ifdef LIGHTING_TYPES_GLSL
	float cloudFloorKM = cloudAltitude * worldScale / 1000.0;
	float gapFactor = 1.0 + humidity * 8.0 * smoothstep(cloudFloorKM + 0.5, cloudFloorKM, h);

	vec3 oldMie = s.mie;
	s.mie *= (humidityFactor * aerosolFactor * gapFactor);
	// Update extinction based on the difference in Mie scattering
	s.extinction += (s.mie - oldMie);
	#else
	vec3 oldMie = s.mie;
	s.mie *= (humidityFactor * aerosolFactor);
	s.extinction += (s.mie - oldMie);
	#endif
#endif

	return s;
}

// Phase Functions
float rayleighPhase(float cosTheta) {
	return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

float miePhase(float cosTheta) {
	float g = u_mieAnisotropy;
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(max(1e-4, 1.0 + g2 - 2.0 * g * cosTheta), 1.5));
}

// LUT mapping functions - Simple Linear mapping for Transmittance to avoid precision issues
vec2 transmittanceToUV(float r, float mu) {
	float x_mu = mu * 0.5 + 0.5;
	float x_r = (r - kEarthRadius) / kAtmosphereHeight;
	return vec2(x_mu, x_r);
}

void UVToTransmittance(vec2 uv, out float r, out float mu) {
	mu = uv.x * 2.0 - 1.0;
	r = kEarthRadius + uv.y * kAtmosphereHeight;
}

#endif
