#ifndef COMMON_TYPES_GLSL
#define COMMON_TYPES_GLSL

// SSBO for decor/foliage instancing (binding 10)
layout(std430, binding = [[DECOR_INSTANCES_BINDING]]) buffer SSBOInstances {
	mat4 ssboInstanceMatrices[];
};

// SSBO for bone matrices (binding 12)
layout(std430, binding = [[BONE_MATRIX_BINDING]]) buffer BoneMatricesSSBO {
	mat4 boneMatrices[];
};

#endif
