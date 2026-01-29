#version 420 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in mat4 aInstanceMatrix;
layout(location = 7) in vec4 aInstanceColor;
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
uniform bool  isLine = false;

void main() {
	vec3 displacedPos = aPos;
	vec3 displacedNormal = aNormal;

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

	mat4 modelMatrix = is_instanced ? aInstanceMatrix : model;
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
