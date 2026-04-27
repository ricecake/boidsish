#version 460 core
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 aPos;
layout(location = 9) in ivec4 aBoneIDs;
layout(location = 10) in vec4 aWeights;

#include "common_uniforms.glsl"

// SSBO for decor/foliage instancing
layout(std430, binding = [[DECOR_INSTANCES_BINDING]]) buffer SSBOInstances {
	mat4 ssboInstanceMatrices[];
};

// SSBO for bone matrices
layout(std430, binding = [[BONE_MATRIX_BINDING]]) buffer BoneMatricesSSBO {
	mat4 boneMatrices[];
};

layout(std430, binding = [[HIERARCHY_PARENTS_BINDING]]) readonly buffer HierarchyParents {
	int h_parents[];
};

layout(std430, binding = [[HIERARCHY_LOCALS_BINDING]]) readonly buffer HierarchyLocals {
	mat4 h_locals[];
};

layout(std430, binding = [[HIERARCHY_INVBIND_BINDING]]) readonly buffer HierarchyInvBinds {
	mat4 h_invBinds[];
};

layout(std430, binding = [[HIERARCHY_STIFFNESS_BINDING]]) readonly buffer HierarchyStiffnesses {
	float h_stiffnesses[];
};

#include "helpers/fast_noise.glsl"
#include "helpers/noise.glsl"
#include "helpers/wind.glsl"
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
	int  current_hierarchy_offset = use_ssbo ? uniforms_data[vUniformIndex].hierarchy_offset : -1;

	mat4 modelMatrix;
	if (current_useSSBOInstancing) {
		modelMatrix = ssboInstanceMatrices[gl_InstanceID];
	} else {
		modelMatrix = current_model;
	}

	vec3 displacedPos = aPos;

	if (current_use_skinning && current_hierarchy_offset >= 0) {
		vec3 seedPos = vec3(modelMatrix[3]);
		float globalSeed = fract(sin(dot(seedPos, vec3(12.9898, 78.233, 45.164))) * 43758.5453);

		vec4  totalPosition = vec4(0.0);
		float totalWeight = 0.0;

		for (int i = 0; i < 4; i++) {
			int boneID = aBoneIDs[i];
			if (boneID < 0 || boneID >= 100)
				continue;

			float weight = aWeights[i];
			if (weight < 0.001) continue;

			mat4 perturbedGlobal = mat4(1.0);
			int curr = boneID;
			int safety = 0;

			while (curr != -1 && safety < 16) {
				int h_idx = current_hierarchy_offset + curr;
				mat4 local = h_locals[h_idx];
				float stiffness = h_stiffnesses[h_idx];

				float limbSeed = fract(sin(float(curr) * 1.618 + globalSeed) * 43758.5453);

				float scaleVariety = 1.0 + (limbSeed - 0.5) * 0.4 * stiffness;
				local[1].xyz *= scaleVariety;

				float angle = (limbSeed - 0.5) * 0.5 * stiffness;
				float s = sin(angle);
				float c = cos(angle);
				vec3 axis = normalize(vec3(limbSeed, 1.0, fract(limbSeed * 7.0)));

				mat3 m_rot = mat3(1.0);
				m_rot[0] = vec3(c + axis.x*axis.x*(1.0-c), axis.x*axis.y*(1.0-c) + axis.z*s, axis.x*axis.z*(1.0-c) - axis.y*s);
				m_rot[1] = vec3(axis.y*axis.x*(1.0-c) - axis.z*s, c + axis.y*axis.y*(1.0-c), axis.y*axis.z*(1.0-c) + axis.x*s);
				m_rot[2] = vec3(axis.z*axis.x*(1.0-c) + axis.y*s, axis.z*axis.y*(1.0-c) - axis.x*s, c + axis.z*axis.z*(1.0-c));

				local = mat4(m_rot) * local;

				perturbedGlobal = local * perturbedGlobal;
				curr = h_parents[h_idx];
				safety++;
			}

			mat4 finalBoneMatrix = perturbedGlobal * h_invBinds[current_hierarchy_offset + boneID];
			totalPosition += (finalBoneMatrix * vec4(displacedPos, 1.0)) * weight;
			totalWeight += weight;
		}

		if (totalWeight > 0.001) {
			displacedPos = totalPosition.xyz / totalWeight;
		}
	} else if (current_use_skinning) {
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

		// Shockwave displacement (applied before wind sway)
		worldPos += getShockwaveDisplacement(instanceCenter, (aPos.y - u_aabbMin.y) * instanceScale, true);

		// Apply wind sway (matches vis.vert logic)
		if (wind_strength > 0.0) {
			float localHeight = max(0.0, aPos.y - u_aabbMin.y);
			float totalHeight = max(0.001, u_aabbMax.y - u_aabbMin.y);
			float normalizedHeight = clamp(localHeight / totalHeight, 0.0, 1.0);

			// 1. Calculate raw wind magnitude and direction from macro wind system
			vec3 windAtPos = getWindAtPosition(instanceCenter);
			// windAtPos is in m/s (up to ~30-40 m/s in storms)
			vec3 rawWindNudge = windAtPos * wind_strength * u_windResponsiveness;

			float windMag = length(rawWindNudge);

			if (windMag > 0.001) {
				vec3 windDir = rawWindNudge / windMag;

				// 2. Apply Asymptotic Resistance (tanh)
				float maxDeflection = 1.3;
				float resistedWindMag = maxDeflection * tanh(windMag * 0.5 / maxDeflection);

				// 3. Calculate bending angle based on resisted wind and height
				float bendAngle = resistedWindMag * pow(normalizedHeight, 1.2) *
					smoothstep(0.05, 1.0, normalizedHeight);

				// 4. Arc Mapping via Rodrigues' Rotation Formula
				vec3 rotationAxis = normalize(cross(vec3(0.0, 1.0, 0.0), windDir));
				vec3 offset = worldPos - worldBaseCenter;

				float cosTheta = cos(bendAngle);
				float sinTheta = sin(bendAngle);

				// Rotate the vertex offset around the base pivot
				vec3 rotatedOffset = offset * cosTheta + cross(rotationAxis, offset) * sinTheta +
					rotationAxis * dot(rotationAxis, offset) * (1.0 - cosTheta);

				worldPos = worldBaseCenter + rotatedOffset;
			}
		}
	}

	gl_Position = lightSpaceMatrix * vec4(worldPos, 1.0);
}
