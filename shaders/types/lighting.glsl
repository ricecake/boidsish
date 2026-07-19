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
	float _pad_clouds0;                      // was cloudShadowIntensity
	vec3  ambient_light;
	float time;
	vec3  viewDir;
	float _pad_after_view_dir;
	vec4  _pad_clouds1[5];
	float _pad_clouds2_a;
	float _pad_clouds2_b;
	float _pad_clouds2_c;
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
	vec4  sh_coeffs[81];
};

#endif
