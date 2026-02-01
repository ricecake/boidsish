#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in mat4 aInstanceMatrix;
layout(location = 7) in vec4  aInstanceColor;
layout(location = 8) in ivec4 boneIds;
layout(location = 9) in vec4  weights;

// SSBO for decor/foliage instancing (binding 10)
layout(std430, binding = 10) buffer SSBOInstances {
	mat4 ssboInstanceMatrices[];
};

#include "frustum.glsl"
#include "helpers/lighting.glsl"
#include "visual_effects.glsl"
#include "visual_effects.vert"

out vec3 FragPos;
out vec3 Normal;
out vec3 vs_color;
out vec3 barycentric;
out vec2 TexCoords;
out vec4 InstanceColor;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;
uniform float ripple_strength;
uniform bool  isColossal = false;
uniform bool  is_instanced = false;
uniform bool  useInstanceColor = false;
uniform bool  useSSBOInstancing = false;
uniform bool  isLine = false;
uniform bool  enableFrustumCulling = false;
uniform float frustumCullRadius = 5.0; // Approximate object radius for sphere test

const int MAX_BONES = [[MAX_BONES]];
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

void main() {
	vec4 totalPosition = vec4(0.0f);
	vec3 totalNormal = vec3(0.0f);

	bool hasBones = false;
	for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
		if (boneIds[i] != -1) {
			hasBones = true;
			vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(aPos, 1.0f);
			totalPosition += localPosition * weights[i];
			vec3 localNormal = mat3(finalBonesMatrices[boneIds[i]]) * aNormal;
			totalNormal += localNormal * weights[i];
		}
	}

	vec3 displacedPos;
	vec3 displacedNormal;

	if (hasBones) {
		displacedPos = totalPosition.xyz;
		displacedNormal = normalize(totalNormal);
	} else {
		displacedPos = aPos;
		displacedNormal = aNormal;
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

	mat4 modelMatrix;
	if (useSSBOInstancing) {
		modelMatrix = ssboInstanceMatrices[gl_InstanceID];
	} else if (is_instanced) {
		modelMatrix = aInstanceMatrix;
	} else {
		modelMatrix = model;
	}

	// Extract world position (translation from model matrix)
	vec3 instanceCenter = vec3(modelMatrix[3]);

	// GPU frustum culling - output degenerate triangle if outside frustum
	if (enableFrustumCulling && !isColossal) {
		// Use sphere test with approximate radius based on scale
		float scale = length(vec3(modelMatrix[0])); // Approximate scale from first column
		float effectiveRadius = frustumCullRadius * scale;

		if (!isSphereInFrustum(instanceCenter, effectiveRadius)) {
			// Output degenerate triangle (all vertices at same point)
			// GPU will automatically cull this
			gl_Position = vec4(0.0, 0.0, -2.0, 1.0); // Behind near plane
			FragPos = vec3(0.0);
			Normal = vec3(0.0, 1.0, 0.0);
			TexCoords = vec2(0.0);
			InstanceColor = vec4(0.0);
			gl_ClipDistance[0] = -1.0; // Clip it
			return;
		}
	}

	FragPos = vec3(modelMatrix * vec4(displacedPos, 1.0));
	Normal = mat3(transpose(inverse(modelMatrix))) * displacedNormal;
	TexCoords = aTexCoords;
	InstanceColor = useInstanceColor ? aInstanceColor : vec4(1.0);
	if (wireframe_enabled == 1) {
		barycentric = getBarycentric();
	}

	if (isColossal) {
		mat4 staticView = mat4(mat3(view));
		vec3 skyPositionOffset = vec3(0.0, -10.0, -500.0);
		vec4 world_pos = model * vec4(displacedPos * 50, 1.0);
		world_pos.xyz += skyPositionOffset;
		gl_Position = projection * staticView * world_pos;
		gl_Position = gl_Position.xyww;
		FragPos = world_pos.xyz;
	} else {
		gl_Position = projection * view * vec4(FragPos, 1.0);
	}

	gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
}
