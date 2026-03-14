#version 460 core
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 aPos;
layout(location = 9) in ivec4 aBoneIDs;
layout(location = 10) in vec4 aWeights;

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

// SSBO for decor/foliage instancing (binding 10)
layout(std430, binding = 10) buffer SSBOInstances {
	mat4 ssboInstanceMatrices[];
};

// SSBO for bone matrices (binding 12)
layout(std430, binding = 12) buffer BoneMatricesSSBO {
	mat4 boneMatrices[];
};

#include "helpers/fast_noise.glsl"
#include "helpers/noise.glsl"
#include "helpers/shockwave.glsl"
#include "lighting.glsl"
#include "temporal_data.glsl"
#include "visual_effects.glsl"

uniform bool uUseMDI = false;
uniform bool useSSBOInstancing = false;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform mat4 finalBonesMatrices[100];
uniform bool use_skinning = false;
uniform int  bone_matrices_offset = -1;

uniform vec3  u_aabbMin;
uniform vec3  u_aabbMax;
uniform float u_windResponsiveness = 1.0;

uniform bool  dissolve_enabled = false;
uniform vec3  dissolve_plane_normal = vec3(0, 1, 0);
uniform float dissolve_plane_dist = 0.0;

out vec3     FragPos;
flat out int vUniformIndex;

void main() {
	int drawID = gl_DrawID;

	vUniformIndex = uUseMDI ? drawID : -1;
	bool use_ssbo = uUseMDI && vUniformIndex >= 0;

	mat4 current_model = use_ssbo ? uniforms_data[vUniformIndex].model : model;
	bool current_useSSBOInstancing = use_ssbo ? (uniforms_data[vUniformIndex].use_ssbo_instancing != 0)
											  : useSSBOInstancing;
	bool current_use_skinning = use_ssbo ? (uniforms_data[vUniformIndex].use_skinning != 0) : use_skinning;
	int  current_bone_offset = use_ssbo ? uniforms_data[vUniformIndex].bone_matrices_offset : bone_matrices_offset;

	mat4 modelMatrix;
	if (current_useSSBOInstancing) {
		modelMatrix = ssboInstanceMatrices[gl_InstanceID];
	} else {
		modelMatrix = current_model;
	}

	vec3 displacedPos = aPos;
	if (current_use_skinning) {
		vec4  totalPosition = vec4(0.0);
		float totalWeight = 0.0;
		for (int i = 0; i < 4; i++) {
			if (aBoneIDs[i] < 0 || aBoneIDs[i] >= 100)
				continue;

			mat4 boneMatrix;
			if (use_ssbo && current_bone_offset >= 0) {
				boneMatrix = boneMatrices[current_bone_offset + aBoneIDs[i]];
			} else {
				boneMatrix = finalBonesMatrices[aBoneIDs[i]];
			}

			totalPosition += (boneMatrix * vec4(aPos, 1.0)) * aWeights[i];
			totalWeight += aWeights[i];
		}
		if (totalWeight > 0.001) {
			displacedPos = totalPosition.xyz / totalWeight;
		}
	}

	vec3 worldPos = vec3(modelMatrix * vec4(displacedPos, 1.0));
	FragPos = worldPos;

	// Apply sway for decor (matches vis.vert logic)
	if (current_useSSBOInstancing) {
		vec3  instanceCenter = vec3(modelMatrix[3]);
		float instanceScale = length(vec3(modelMatrix[0]));

		// Calculate the center of the base of the AABB in world space
		vec3 localBaseCenter = vec3((u_aabbMin.x + u_aabbMax.x) * 0.5, u_aabbMin.y, (u_aabbMin.z + u_aabbMax.z) * 0.5);
		vec3 worldBaseCenter = vec3(modelMatrix * vec4(localBaseCenter, 1.0));

		// Distance from vertex to base center before swaying
		float distToCenter = distance(worldPos, worldBaseCenter);

		// Shockwave displacement
		worldPos += getShockwaveDisplacement(instanceCenter, (aPos.y - u_aabbMin.y) * instanceScale, true);

		// Wind sway
		if (wind_strength > 0.0) {
			float localHeight = max(0.0, aPos.y - u_aabbMin.y);
			float totalHeight = max(0.001, u_aabbMax.y - u_aabbMin.y);
			float normalizedHeight = clamp(localHeight / totalHeight, 0.0, 1.0);

			float fateFactor = fastWorley3d(vec3(instanceCenter.xz / 10, time * 0.5)) * 0.5 + 0.5;
			vec2  windNudge = fateFactor * curlNoise2D(instanceCenter.xz * wind_frequency + time * wind_speed * 0.5) *
				wind_strength * u_windResponsiveness;
			worldPos.xz += windNudge * normalizedHeight * pow(normalizedHeight, 1.2);
		}

		// Re-normalize to maintain original distance from base center (bowing effect)
		vec3  direction = worldPos - worldBaseCenter;
		float newDist = length(direction);
		if (newDist > 0.001) {
			worldPos = worldBaseCenter + (direction / newDist) * distToCenter;
		}
	}

	gl_Position = lightSpaceMatrix * vec4(worldPos, 1.0);
}
