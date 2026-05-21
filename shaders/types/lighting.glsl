#ifndef LIGHTING_TYPES_GLSL
#define LIGHTING_TYPES_GLSL

struct Light {
	vec3  position;
	float intensity;
	vec3  color;
	int   type;
	vec3  direction;
	float inner_cutoff; // Also: emissive_radius (EMISSIVE), flash_radius (FLASH)
	float outer_cutoff; // Also: falloff_exp (FLASH)
};

struct SurfaceData {
	vec3 pos;
	vec3 normal;
	vec3 viewDir;
	vec2 uv;
	mat2 uv_J;
};

struct MaterialData {
	vec3  albedo;
	float roughness;
	float metallic;
	float ao;
	float translucency;
	float glintIntensity;
	float iridescenceThickness;
	float iridescenceIntensity;
};

struct ShadingData {
	vec3  V;
	vec3  N;
	vec3  L;
	vec3  H;
	float NoV;
	float NoL;
	float NoH;
	float LoH;
	vec3  F0;
};

struct LightingResult {
	vec3  color;
	float specLum;
	float primaryShadow;
};

const int MAX_LIGHTS = [[MAX_LIGHTS]];

layout(std140, binding = [[LIGHTING_BINDING]]) uniform Lighting {
	Light lights[MAX_LIGHTS];
	int   num_lights;
	float worldScale;
	float dayTime;
	float nightFactor;
	vec3  viewPos;
	float cloudShadowIntensity;
	vec3  ambient_light;
	float time;
	vec3  viewDir;
	float cloudAltitude;
	float cloudThickness;
	float cloudDensity;
	float cloudCoverage;
	float cloudWarp;
	float cloudPhaseG1;
	float cloudPhaseG2;
	float cloudPhaseAlpha;
	float cloudPhaseIsotropic;
	float cloudPowderScale;
	float cloudPowderMultiplier;
	float cloudPowderLocalScale;
	float cloudShadowOpticalDepthMultiplier;
	float cloudShadowStepMultiplier;
	float cloudSunLightScale;
	float cloudMoonLightScale;
	float cloudBeerPowderMix;
	mat4  cloudShadowMatrix;
	vec4  sh_coeffs[9];
};

#endif
