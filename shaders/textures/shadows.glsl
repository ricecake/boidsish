#ifndef SHADOW_TEXTURES_GLSL
#define SHADOW_TEXTURES_GLSL

#include "types/shadows.glsl"

// Shadow map texture array
layout(binding = [[SHADOW_MAPS_BINDING]]) uniform sampler2DArrayShadow shadowMaps;

#endif
