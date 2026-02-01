#version 420 core

layout(location = 0) in vec3  aPos;
layout(location = 8) in ivec4 boneIds;
layout(location = 9) in vec4  weights;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

const int MAX_BONES = [[MAX_BONES]];
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

void main() {
	vec4 totalPosition = vec4(0.0f);
	bool hasBones = false;
	for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
		if (boneIds[i] != -1) {
			hasBones = true;
			vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(aPos, 1.0f);
			totalPosition += localPosition * weights[i];
		}
	}

	if (!hasBones) {
		totalPosition = vec4(aPos, 1.0);
	}

	gl_Position = lightSpaceMatrix * model * totalPosition;
}
