#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

#include "common_uniforms.glsl"
#include "temporal_data.glsl"
#include "visual_effects.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;

out vec3     FragPos;
out vec3     Normal;
out vec2     TexCoords;
out vec4     CurPosition;
out vec4     PrevPosition;
flat out int vUniformIndex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 clipPlane;

void main() {
	int drawID = gl_DrawID;

	vUniformIndex = uUseMDI ? drawID : -1;
	bool use_ssbo = uUseMDI && vUniformIndex >= 0;
	mat4 current_model = use_ssbo ? uniforms_data[vUniformIndex].model : model;

	FragPos = vec3(current_model * vec4(aPos, 1.0));
	Normal = mat3(transpose(inverse(current_model))) * aNormal;
	TexCoords = aTexCoords;

	vec4 vPos = view * vec4(FragPos, 1.0);
	vPos = warpViewSpace(vPos);
	gl_Position = projection * vPos;

	CurPosition = gl_Position;
	vec4 prevVPos = prevView * vec4(FragPos, 1.0);
	prevVPos = warpViewSpace(prevVPos);
	PrevPosition = uProjection * prevVPos;
	gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
}
