#version 420 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

#include "artistic_effects.vert"
#include "artistic_effects.glsl"

out vec3 FragPos;
out vec3 Normal;
out vec3 vs_color;
out vec3 barycentric;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;

layout(std140, binding = 1) uniform VisualEffects {
    bool  ripple_enabled;
    float ripple_strength;
    bool  color_shift_enabled;
    float color_shift_strength;
};

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

void main() {
	vec3 displacedPos = aPos;
	vec3 displacedNormal = aNormal;

	displacedPos = applyGlitch(displacedPos, time);

	if (ripple_enabled) {
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

	FragPos = vec3(model * vec4(displacedPos, 1.0));
	Normal = mat3(transpose(inverse(model))) * displacedNormal;
	vs_color = aColor;
	barycentric = getBarycentric();
	gl_Position = projection * view * vec4(FragPos, 1.0);
	gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
}
