#version 460 core
#extension GL_ARB_shader_draw_parameters : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 8) in vec3 aVertexColor;
layout(location = 9) in ivec4 aBoneIDs;
layout(location = 10) in vec4 aWeights;

#include "common_uniforms.glsl"

uniform bool uUseMDI = false;

// SSBO for decor/foliage instancing
layout(std430, binding = [[DECOR_INSTANCES_BINDING]]) buffer SSBOInstances {
	mat4 ssboInstanceMatrices[];
};

// SSBO for bone matrices
layout(std430, binding = [[BONE_MATRIX_BINDING]]) buffer BoneMatricesSSBO {
	mat4 boneMatrices[];
};

#include "frustum.glsl"
#include "temporal_data.glsl"

out vec3 vPos;
out vec3 vNormal;
out vec2 vTexCoords;
out vec3 vVertexColor;
flat out ivec4 vBoneIDs;
flat out vec4 vWeights;
flat out int vDrawID;
flat out int vInstanceID;

void main() {
	int drawID = gl_DrawIDARB;
	vDrawID = drawID;
	vInstanceID = gl_InstanceID;

	vPos = aPos;
	vNormal = aNormal;
	vTexCoords = aTexCoords;
	vVertexColor = aVertexColor;
	vBoneIDs = aBoneIDs;
	vWeights = aWeights;

	gl_Position = vec4(aPos, 1.0);
}
