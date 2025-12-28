#version 330 core
out vec4 FragColor;

in vec3 vs_color;
in vec3 vs_normal;
in vec3 vs_frag_pos;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

uniform bool useIridescence;

void main() {
	vec3 norm = normalize(vs_normal);
	vec3 view_dir = normalize(viewPos - vs_frag_pos);
	vec3 light_dir = normalize(lightPos - vs_frag_pos);

	if (useIridescence) {
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
