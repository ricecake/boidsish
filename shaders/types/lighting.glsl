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
	int   flags;
	float _pad0;
	float _pad1;
};

const int LIGHT_FLAG_CASTS_SHADOW = 1;
const int LIGHT_FLAG_VOLUMETRIC_SHADOW = 2;
const int LIGHT_FLAG_CAMERA_RELATIVE = 4;
const int LIGHT_FLAG_CLOUD_EMISSIVE = 8;

const int MAX_LIGHTS = [[MAX_LIGHTS]];

layout(std140, binding = [[LIGHTING_BINDING]]) uniform Lighting {
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
	float cloudFlowSpeed;
	float cloudFlowDirection;
	float cloudFlowHeightScale;
	float cloudCurlStrength;
	float cloudCurlFrequency;
	float sunAureoleStrength;
	float cirrusOpacity;
	float zNear;
	float zFar;
	float _pad_clouds3_a;
	float _pad_clouds3_b;
	float _pad_clouds3_c;
	vec4  _pad_clouds3_vec[3];
	vec4  _pad_cloud_shadow_mat[4];
	mat4  view;
	mat4  projection;
	vec3  lightningColor;
	float lightningPulse;
	float skyExposure;
	float starExposure;
	float terrainExposure;
	float _pad_exposure;
	vec4  sh_coeffs[81];
};

#endif
