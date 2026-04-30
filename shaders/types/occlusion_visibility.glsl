#ifndef OCCLUSION_VISIBILITY_TYPES_GLSL
#define OCCLUSION_VISIBILITY_TYPES_GLSL

#ifdef COMPUTE_SHADER
layout(std430, binding = [[OCCLUSION_VISIBILITY_BINDING]]) buffer OcclusionVisibility {
	uint hiz_visibility[];
};
#else
layout(std430, binding = [[OCCLUSION_VISIBILITY_BINDING]]) readonly buffer OcclusionVisibility {
	uint hiz_visibility[];
};
#endif

#endif
