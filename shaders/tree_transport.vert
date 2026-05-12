#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 8) in vec3 aMetadata; // R=depth, G=branch_factor, B=leaf_factor

#include "common_uniforms.glsl"

layout(std430, binding = [[DECOR_INSTANCES_BINDING]]) buffer SSBOInstances {
	mat4 ssboInstanceMatrices[];
};

#include "helpers/wind.glsl"
#include "temporal_data.glsl"

out vec3 vPos;
out vec3 vNormal;
out vec2 vTexCoords;
out vec3 vMetadata;
flat out int vInstanceID;

uniform bool useSSBOInstancing = false;
uniform mat4 model;

void main() {
	vInstanceID = gl_InstanceID;
	mat4 modelMatrix = useSSBOInstancing ? ssboInstanceMatrices[gl_InstanceID] : model;

	vMetadata = aMetadata;
	vTexCoords = aTexCoords;

	// Hierarchical Wind Sway
	float depth = aMetadata.r;
	float branchFactor = aMetadata.g;
	float isLeaf = aMetadata.b;

	vec3 worldPos = vec3(modelMatrix * vec4(aPos, 1.0));
	vec3 wind = getWindAtPosition(worldPos);

	// Trunk is stable, ends flutter.
	// branchFactor is cumulative length from root.
	float swayAmount = branchFactor * 0.05 * wind_strength;
	float flutter = isLeaf * 0.1 * sin(time * 10.0 + dot(worldPos, vec3(1.0))) * wind_strength;

	vec3 offset = wind * (swayAmount + flutter);

	vPos = worldPos + offset;
	vNormal = mat3(transpose(inverse(modelMatrix))) * aNormal;

	gl_Position = vec4(vPos, 1.0);
}
