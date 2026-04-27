#version 460 core

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

#include "frustum.glsl"
#include "helpers/fast_noise.glsl"
#include "helpers/wind.glsl"
#include "helpers/lighting.glsl"
#include "helpers/shockwave.glsl"
#include "temporal_data.glsl"
#include "visual_effects.glsl"
#include "visual_effects.vert"

out vec3     FragPos;
out vec4     CurPosition;
out vec4     PrevPosition;
out vec3     Normal;
out vec3     vs_color;
out vec3     barycentric;
out vec2     TexCoords;
out vec4     InstanceColor;
out float    WindDeflection;
flat out int vUniformIndex;

uniform mat4  model;
uniform mat4  finalBonesMatrices[100];
uniform bool  use_skinning = false;
uniform int   bone_matrices_offset = -1;
uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;
uniform float ripple_strength;
uniform bool  isColossal = false;
uniform bool  useSSBOInstancing = false;
uniform bool  isLine = false;
uniform bool  enableFrustumCulling = false;
uniform float frustumCullRadius = 5.0; // Approximate object radius for sphere test
uniform bool  enableHiZCulling = false;

// Hi-Z occlusion visibility (per-draw, written by occlusion_cull.comp)
layout(std430, binding = [[OCCLUSION_VISIBILITY_BINDING]]) readonly buffer OcclusionVisibility {
	uint hiz_visibility[];
};

uniform uint u_baseVisibilityIndex;

uniform vec3  u_aabbMin;
uniform vec3  u_aabbMax;
uniform float u_windResponsiveness;

// Arcade Text Effects
uniform bool  isArcadeText = false;
uniform int   arcadeWaveMode = 0; // 0: None, 1: Vertical, 2: Flag, 3: Twist
uniform float arcadeWaveAmplitude = 0.5;
uniform float arcadeWaveFrequency = 10.0;
uniform float arcadeWaveSpeed = 5.0;

void main() {
	int drawID = gl_DrawID;

	vUniformIndex = uUseMDI ? drawID : -1;
	bool use_ssbo = uUseMDI && vUniformIndex >= 0;

	mat4  current_model = use_ssbo ? uniforms_data[vUniformIndex].model : model;
	bool  current_use_skinning = use_ssbo ? (uniforms_data[vUniformIndex].use_skinning != 0) : use_skinning;
	int   current_bone_offset = use_ssbo ? uniforms_data[vUniformIndex].bone_matrices_offset : bone_matrices_offset;
	bool  current_isArcadeText = use_ssbo ? (uniforms_data[vUniformIndex].is_arcade_text != 0) : isArcadeText;
	int   current_arcadeWaveMode = use_ssbo ? uniforms_data[vUniformIndex].arcade_wave_mode : arcadeWaveMode;
	float current_arcadeWaveAmplitude = use_ssbo ? uniforms_data[vUniformIndex].arcade_wave_amplitude
												 : arcadeWaveAmplitude;
	float current_arcadeWaveFrequency = use_ssbo ? uniforms_data[vUniformIndex].arcade_wave_frequency
												 : arcadeWaveFrequency;
	float current_arcadeWaveSpeed = use_ssbo ? uniforms_data[vUniformIndex].arcade_wave_speed : arcadeWaveSpeed;
	bool  current_isColossal = use_ssbo ? (uniforms_data[vUniformIndex].is_colossal != 0) : isColossal;
	bool  current_useSSBOInstancing = use_ssbo ? (uniforms_data[vUniformIndex].use_ssbo_instancing != 0)
											   : useSSBOInstancing;

	WindDeflection = 0.0;
	vec3 displacedPos = aPos;
	vec3 displacedNormal = aNormal;

	int current_hierarchy_offset = use_ssbo ? uniforms_data[vUniformIndex].hierarchy_offset : -1;

	mat4 modelMatrix;
	if (current_useSSBOInstancing) {
		modelMatrix = ssboInstanceMatrices[gl_InstanceID];
	} else {
		modelMatrix = current_model;
	}

	if (current_use_skinning && current_hierarchy_offset >= 0) {
		vec3 seedPos = vec3(modelMatrix[3]);
		float globalSeed = fract(sin(dot(seedPos, vec3(12.9898, 78.233, 45.164))) * 43758.5453);

		vec4  totalPosition = vec4(0.0);
		vec3  totalNormal = vec3(0.0);
		float totalWeight = 0.0;

		for (int i = 0; i < 4; i++) {
			int boneID = aBoneIDs[i];
			if (boneID < 0 || boneID >= 100)
				continue;

			float weight = aWeights[i];
			if (weight < 0.001) continue;

			// Compute hierarchical perturbed transform
			mat4 perturbedGlobal = mat4(1.0);
			int curr = boneID;
			int safety = 0;

			// Compute chain of transforms up to root
			while (curr != -1 && safety < 16) {
				int h_idx = current_hierarchy_offset + curr;
				mat4 local = h_locals[h_idx];
				float stiffness = h_stiffnesses[h_idx];

				// Procedural variety for this specific limb node
				float limbSeed = fract(sin(float(curr) * 1.618 + globalSeed) * 43758.5453);

				// Apply limb scaling variety (length)
				float scaleVariety = 1.0 + (limbSeed - 0.5) * 0.4 * stiffness;
				local[1].xyz *= scaleVariety; // Scale along Y (assumed bone direction)

				// Apply limb rotation variety
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
			totalNormal += (mat3(finalBoneMatrix) * displacedNormal) * weight;
			totalWeight += weight;
		}

		if (totalWeight > 0.001) {
			displacedPos = totalPosition.xyz / totalWeight;
			displacedNormal = normalize(totalNormal);
		}
	} else if (current_use_skinning) {
		vec4  totalPosition = vec4(0.0);
		vec3  totalNormal = vec3(0.0);
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
			totalNormal += (mat3(boneMatrix) * aNormal) * aWeights[i];
			totalWeight += aWeights[i];
		}
		if (totalWeight > 0.001) {
			displacedPos = totalPosition.xyz / totalWeight;
			displacedNormal = normalize(totalNormal);
		}
	}

	if (current_isArcadeText) {
		float x = aTexCoords.x;
		float phase = time * current_arcadeWaveSpeed;

		if (current_arcadeWaveMode == 1) { // Vertical Rippling Wave
			displacedPos.y += sin(x * current_arcadeWaveFrequency + phase) * current_arcadeWaveAmplitude;
		} else if (current_arcadeWaveMode == 2) { // Flag Style Wave
			displacedPos.y += sin(x * current_arcadeWaveFrequency + phase) * current_arcadeWaveAmplitude;
			displacedPos.z += cos(x * current_arcadeWaveFrequency * 0.7 + phase * 0.8) * current_arcadeWaveAmplitude *
				0.5;
		} else if (current_arcadeWaveMode == 3) { // Lengthwise Twist
			float twist_angle = (x - 0.5) * current_arcadeWaveAmplitude * sin(phase);
			float s = sin(twist_angle);
			float c = cos(twist_angle);
			// Rotate around local X axis
			float y = displacedPos.y;
			float z = displacedPos.z;
			displacedPos.y = y * c - z * s;
			displacedPos.z = y * s + z * c;
		}
	}

	if (glitched_enabled == 1) {
		displacedPos = applyGlitch(displacedPos, time);
	}

	if (ripple_strength > 0.0) {
		float frequency = 20.0;
		float speed = 3.0;
		float amplitude = ripple_strength;

		float wave = sin(frequency * (aPos.x + aPos.z) + time * speed);
		displacedPos = aPos + aNormal * wave * amplitude;

		vec3 gradient = vec3(
			cos(frequency * (aPos.x + aPos.z) + time * speed) * frequency * amplitude,
			0.0,
			cos(frequency * (aPos.x + aPos.z) + time * speed) * frequency * amplitude
		);
		displacedNormal = normalize(aNormal - gradient);
	}


	// Extract world position (translation from model matrix)
	vec3  instanceCenter = vec3(modelMatrix[3]);
	float instanceScale = length(vec3(modelMatrix[0])); // Approximate scale from first column

	// GPU frustum culling - output degenerate triangle if outside frustum
	if (enableFrustumCulling && !current_isColossal) {
		bool inFrustum = true;

		if (use_ssbo) {
			// Use the accurate AABB from the SSBO if available
			vec3 aabbMin = vec3(uniforms_data[vUniformIndex].aabb_min_x, uniforms_data[vUniformIndex].aabb_min_y,
								uniforms_data[vUniformIndex].aabb_min_z);
			vec3 aabbMax = vec3(uniforms_data[vUniformIndex].aabb_max_x, uniforms_data[vUniformIndex].aabb_max_y,
								uniforms_data[vUniformIndex].aabb_max_z);

			// Check for degenerate AABB (signals no culling desired or unset)
			if (aabbMin != aabbMax) {
				inFrustum = isAABBInFrustum(aabbMin, aabbMax);
			} else {
				inFrustum = isSphereInFrustum(instanceCenter, frustumCullRadius * instanceScale);
			}
		} else {
			// Fallback to sphere test for immediate rendering
			inFrustum = isSphereInFrustum(instanceCenter, frustumCullRadius * instanceScale);
		}

		if (!inFrustum) {
			// Output degenerate triangle (all vertices at same point)
			// GPU will automatically cull this
			gl_Position = vec4(0.0, 0.0, -2.0, 1.0); // Behind near plane
			FragPos = vec3(0.0);
			Normal = vec3(0.0, 1.0, 0.0);
			TexCoords = vec2(0.0);
			gl_ClipDistance[0] = -1.0; // Clip it
			return;
		}
	}

	// Hi-Z occlusion culling - output degenerate triangle if occluded by previous frame's depth
	if (enableHiZCulling && uUseMDI && !current_isColossal) {
		if (hiz_visibility[u_baseVisibilityIndex + drawID] == 0u) {
			gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
			FragPos = vec3(0.0);
			Normal = vec3(0.0, 1.0, 0.0);
			TexCoords = vec2(0.0);
			gl_ClipDistance[0] = -1.0;
			return;
		}
	}

	FragPos = vec3(modelMatrix * vec4(displacedPos, 1.0));

	// Apply shockwave displacement (sway for decor)
	// Calculate at instanceCenter to prevent warping, scale by world-relative height
	if (current_useSSBOInstancing) {
		// Calculate the center of the base in world space (the pivot point)
		vec3 localBaseCenter = vec3((u_aabbMin.x + u_aabbMax.x) * 0.5, u_aabbMin.y, (u_aabbMin.z + u_aabbMax.z) * 0.5);
		vec3 worldBaseCenter = vec3(modelMatrix * vec4(localBaseCenter, 1.0));

		// Shockwave displacement (applied before wind sway)
		FragPos += getShockwaveDisplacement(instanceCenter, (aPos.y - u_aabbMin.y) * instanceScale, true);

		// Apply wind sway
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
				// Limits maximum deflection so the tree never folds completely flat
				float maxDeflection = 1.1; // Allow more deflection for high-speed macro wind
				// wind_strength (0.01-0.5) * windAtPos (0-40) ~ 0-20.
				// We scale this to a reasonable radian angle. Increase multiplier for visibility.
				float resistedWindMag = maxDeflection * tanh(windMag * 0.15 / maxDeflection);

				// WindDeflection = resistedWindMag;

				// 3. Calculate bending angle based on resisted wind and height
				float bendAngle = resistedWindMag * pow(normalizedHeight, 1.2) *
					smoothstep(0.05, 1.0, normalizedHeight);

				// --- SECONDARY FLUTTER (Directional Displacement) ---
				// 1. Branch Factor: Isolate the canopy from the trunk
				vec2 trunkCenterXZ = (u_aabbMin.xz + u_aabbMax.xz) * 0.5;
				float distFromTrunk = length(aPos.xz - trunkCenterXZ);
				float maxRadius = max(0.001, (u_aabbMax.x - u_aabbMin.x) * 0.5);

				// Dead-zone near the trunk (e.g., inner 15%), scaling up to 1.0 at the AABB edge
				float branchFactor = smoothstep(maxRadius * 0.15, maxRadius, distFromTrunk);

				// 2. High-Frequency Flutter
				// Multipliers here simulate the rapid rustling of lighter geometry.
				// Dotting aPos with an arbitrary vector provides a quick spatial phase offset.
				float flutterSpeed = wind_speed * 4.0;
				float phase = dot(aPos, vec3(1.3, 0.7, 1.1));

				// Small amplitude multiplier (0.05) prevents the mesh from tearing
				float flutterMag = sin(time * flutterSpeed + phase) * branchFactor * wind_strength * 0.05;

				// 3. Apply Displacement
				// Pushing along the vertex normal is standard for this tier of detail.
				vec3 flutterOffset = aNormal * flutterMag;

				// Base offset for the trunk rotation, now including the local flutter
				vec3 offset = (FragPos - worldBaseCenter) + flutterOffset;


				// --- PRIMARY TRUNK BEND (Rodrigues' Rotation) ---
				// Find the axis perpendicular to both Up and the Wind direction
				vec3 rotationAxis = normalize(cross(vec3(0.0, 1.0, 0.0), windDir));

				float cosTheta = cos(bendAngle);
				float sinTheta = sin(bendAngle);

				// Rotate the combined offset around the base pivot
				vec3 rotatedOffset = offset * cosTheta + cross(rotationAxis, offset) * sinTheta +
									rotationAxis * dot(rotationAxis, offset) * (1.0 - cosTheta);

				FragPos = worldBaseCenter + rotatedOffset;			}
		}
	}

	Normal = mat3(transpose(inverse(modelMatrix))) * displacedNormal;
	TexCoords = aTexCoords;
	vs_color = aVertexColor;
	// if (wireframe_enabled == 1) {
	barycentric = getBarycentric();
	// }

	if (current_isColossal) {
		// Skybox-like rendering: rotation-only view removes translation so the
		// object stays at a fixed direction in the sky (no parallax, infinite distance)
		mat4 staticView = mat4(mat3(view));
		vec3 skyPositionOffset = vec3(0.0, -10.0, -500.0);
		vec4 world_pos = current_model * vec4(displacedPos * 50.0, 1.0);
		world_pos.xyz += skyPositionOffset;
		gl_Position = projection * staticView * world_pos;
		gl_Position.z = gl_Position.w * 0.99999;
		FragPos = world_pos.xyz;

		CurPosition = gl_Position;
		PrevPosition =
			gl_Position; // Sky box doesn't move relative to camera normally, or we don't care about its velocity
	} else {
		gl_Position = projection * view * vec4(FragPos, 1.0);
		CurPosition = gl_Position;
		// Since we don't have previous model matrix, we assume static objects for velocity
		// This is sufficient for GTAO and most environment reprojection.
		PrevPosition = prevViewProjection * vec4(FragPos, 1.0);
	}

	gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
}
