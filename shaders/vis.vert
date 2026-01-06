#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

#include "visual_effects.glsl"
#include "visual_effects.vert"

out vec3 FragPos;
out vec3 Normal;
out vec3 vs_color;
out vec3 barycentric;
out vec2 TexCoords;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;
uniform float ripple_strength;
uniform bool  isColossal;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

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

	FragPos = vec3(model * vec4(displacedPos, 1.0));
	Normal = mat3(transpose(inverse(model))) * displacedNormal;
	TexCoords = aTexCoords;
	if (wireframe_enabled == 1) {
		barycentric = getBarycentric();
	}

	if (isColossal) {
		// --- Colossal Object Logic ---
		// 1. Greatly increase the scale
		mat4 scaled_model = model;
		scaled_model[0][0] *= 500.0;
		scaled_model[1][1] *= 500.0;
		scaled_model[2][2] *= 500.0;

		// 2. Place it far away, relative to the camera's rotation but not its position
		// This makes it look like it's infinitely far away.
		// We extract the camera's forward direction from the view matrix.
		vec3 cam_forward = -normalize(view[2].xyz);
		vec3 pos = cam_forward * 990.0; // Push it just inside the far clip plane

		// 3. Apply the final transformation
		vec4 worldPos = scaled_model * vec4(displacedPos, 1.0);
		worldPos.xyz += pos;
		gl_Position = projection * view * worldPos;

		// 4. Force depth to be the furthest possible value, ensuring it's behind everything.
		gl_Position.z = gl_Position.w;
		FragPos = worldPos.xyz; // Still pass world position for lighting
	} else {
		// --- Standard Object Logic ---
		gl_Position = projection * view * vec4(FragPos, 1.0);
	}

	gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
}
