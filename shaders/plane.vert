#version 330 core
layout(location = 0) in vec3 aPos;

out vec3 WorldPos;
out vec3 Normal;
out vec4 ReflectionClipSpacePos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 reflectionViewProjection;

#include "visual_effects.glsl"

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

void main() {
	vec3 modifiedPos = aPos;
	vec3 modifiedNormal = vec3(0.0, 1.0, 0.0);

	if (distant_curl_enabled == 1) {
		// We need to know the world position to calculate distance fade, but we need to modify the model position.
		// So we do a temporary transform to get world pos.
		vec3 tempWorldPos = vec3(model * vec4(aPos, 1.0));
		tempWorldPos.xz += viewPos.xz;

		float dist = length(tempWorldPos.xz - viewPos.xz);
		float curl_intensity = smoothstep(distant_curl_fade_start, distant_curl_fade_end, dist);

		if (curl_intensity > 0.01) { // Epsilon check to avoid unnecessary calculations
			float radial_dist = length(aPos.xz);

			// Apply y-offset in model space
			modifiedPos.y += curl_intensity * pow(radial_dist, 2.0) * distant_curl_strength;

			// Calculate the derivative of the y-offset function: f(x,z) = C * (x^2 + z^2)
			// where C = curl_intensity * curl_strength
			float C = curl_intensity * curl_strength;
			float df_dx = 2.0 * C * aPos.x;
			float df_dz = 2.0 * C * aPos.z;

			// The normal in model space is normalize(vec3(-df_dx, 1.0, -df_dz))
			modifiedNormal = normalize(vec3(-df_dx, 1.0, -df_dz));
		}
	}

	WorldPos = vec3(model * vec4(modifiedPos, 1.0));
	WorldPos.xz += viewPos.xz;

	// Transform normal to world space
	Normal = mat3(transpose(inverse(model))) * modifiedNormal;

	ReflectionClipSpacePos = reflectionViewProjection * vec4(WorldPos, 1.0);
	gl_Position = projection * view * vec4(WorldPos, 1.0);
}
