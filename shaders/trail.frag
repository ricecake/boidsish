#version 330 core
out vec4 FragColor;

in vec3  vs_color;
in vec3  vs_normal;
in vec3  vs_frag_pos;
in float vs_progress;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

uniform bool useIridescence;
uniform bool useRocketTrail;
uniform bool useCondensationTrail;

#include "helpers/noise.glsl"

void main() {
	// FragColor = vec4(vs_progress, 0, 0, 1);
	// return;
	vec3 norm = normalize(vs_normal);
	vec3 view_dir = normalize(viewPos - vs_frag_pos);
	vec3 light_dir = normalize(lightPos - vs_frag_pos);

	if (useRocketTrail) {
		// --- Rocket Trail Effect ---
		float flame_threshold = 0.9;
		float flicker = snoise(vec2(time * 20.0, vs_frag_pos.x * 5.0)) * 0.5 + 0.5;

		if (vs_progress > flame_threshold) {
			// --- Flame ---
			float flame_progress = (vs_progress - flame_threshold) / (1.0 - flame_threshold);

			// Core of the flame (white hot and flickering)
			float core_intensity = pow(flame_progress, 10.0) * flicker * 2.0;
			vec3  flame_color = vec3(1.0, 1.0, 0.8) * core_intensity;

			// Softer outer glow (orange/yellow)
			float glow_intensity = pow(flame_progress, 2.0) * (1.0 - core_intensity) * 1.5;
			flame_color += vec3(1.0, 0.6, 0.2) * glow_intensity;

			FragColor = vec4(flame_color, 1.0);
		} else {
			// --- Smoke ---
			float smoke_progress = vs_progress / flame_threshold;

			// Base smoke color is a mix of entity color and grey
			vec3 smoke_color = mix(vec3(0.5), vs_color, 0.3);

			// Noise to create a cloudy/billowing texture
			float noise = snoise(vec2(vs_progress * 5.0, time * 2.0)) * 0.5 + 0.5;

			// Alpha fades out at the tail and is modulated by noise
			float alpha = smoothstep(0.0, 0.2, smoke_progress) * (0.4 + noise * 0.3);

			FragColor = vec4(smoke_color, alpha);
		}
	} else if (useIridescence) {
		// --- Iridescence Effect ---
		// Fresnel term for the base reflectivity
		float fresnel = pow(1.0 - abs(dot(view_dir, norm)), 5.0);

		// Use view angle to create a color shift
		float angle_factor = 1.0 - abs(dot(view_dir, norm));
		angle_factor = pow(angle_factor, 2.0);

		// Use time and fragment position to create a swirling effect
		float swirl = sin(time * 0.5 + vs_frag_pos.y * 2.0) * 0.5 + 0.5;

		// Combine for final color using a rainbow palette shifted by the swirl and angle
		vec3 iridescent_color = vec3(
			sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5,
			sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5,
			sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5
		);

		// Add a strong specular highlight
		vec3  reflect_dir = reflect(-light_dir, norm);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 128.0);
		vec3  specular = 1.5 * spec * vec3(1.0); // white highlight

		vec3 final_color = mix(iridescent_color, vec3(1.0), fresnel) + specular;

		FragColor = vec4(final_color, 0.75); // Semi-transparent
	} else if (useCondensationTrail) {
		// --- Condensation Trail Effect ---
		// Base color is a solid bluish-white
		vec3 base_color = vec3(0.8, 0.9, 1.0);

		// Make the trail more transparent at the top and bottom
		float transparency = 1.0 - abs(vs_normal.y);
		transparency = pow(transparency, 2.0);

		// Add some gentle noise to the transparency
		float noise = snoise(vec2(vs_progress * 10.0, time * 5.0)) * 0.5 + 0.5;
		transparency -= noise * 0.2;

		// Fade out the trail at the end
		float alpha = smoothstep(0.0, 0.2, vs_progress) * transparency;

		FragColor = vec4(base_color, alpha);
	} else {
		// --- Original Phong Lighting ---
		// Ambient
		float ambient_strength = 0.2;
		vec3  ambient = ambient_strength * lightColor;

		// Diffuse
		float diff = max(dot(norm, light_dir), 0.0);
		vec3  diffuse = diff * lightColor;

		// Specular
		float specular_strength = 0.5;
		vec3  reflect_dir = reflect(-light_dir, norm);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular = specular_strength * spec * lightColor;

		vec3 result = (ambient + diffuse) * vs_color + specular;
		FragColor = vec4(result, 1.0);
	}
}
