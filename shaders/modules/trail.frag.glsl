//- Fragment shader logic for trails
void fragment_trail() {
	// Ambient
	float ambient_strength = 0.2;
	vec3  ambient = ambient_strength * lightColor;

	// Diffuse
	vec3  norm = normalize(vs_Normal);
	vec3  light_dir = normalize(lightPos - vs_FragPos);
	float diff = max(dot(norm, light_dir), 0.0);
	vec3  diffuse = diff * lightColor;

	// Specular
	float specular_strength = 0.5;
	vec3  view_dir = normalize(viewPos - vs_FragPos);
	vec3  reflect_dir = reflect(-light_dir, norm);
	float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
	vec3  specular = specular_strength * spec * lightColor;

	vec3 result = (ambient + diffuse) * vs_color + specular;
	FragColor = vec4(result, 1.0);
}
