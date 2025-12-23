//- Fragment shader logic for standard visualization
void fragment_vis() {
	// Ambient
	float ambientStrength = 0.1;
	vec3  ambient = ambientStrength * lightColor;

	// Diffuse
	vec3  norm = normalize(vs_Normal);
	vec3  lightDir = normalize(lightPos - vs_FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3  diffuse = diff * lightColor;

	// Specular
	float specularStrength = 1.0;
	vec3  viewDir = normalize(viewPos - vs_FragPos);
	vec3  reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
	vec3  specular = specularStrength * spec * lightColor;

	// Rim
	float rimPower = 2.0;
	float rim = 1.0 - max(dot(viewDir, norm), 0.0);
	rim = pow(rim, rimPower);
	vec3 rimColor = rim * lightColor;

	vec3 final_color;
	if (useVertexColor == 1) {
		final_color = vs_color;
	} else {
		final_color = objectColor;
	}

	vec3 result = (ambient + diffuse) * final_color + specular;

	if (colorShift) {
		float shift_magnitude = 0.2;
		float shift_speed = 5.0;
		vec3  pos_based_shift;
		pos_based_shift.r = sin(vs_FragPos.x * shift_speed) * shift_magnitude;
		pos_based_shift.g = sin(vs_FragPos.y * shift_speed) * shift_magnitude;
		pos_based_shift.b = sin(vs_FragPos.z * shift_speed) * shift_magnitude;
		result += pos_based_shift;

		int posterize_levels = 5;
		result.r = floor(result.r * posterize_levels) / posterize_levels;
		result.g = floor(result.g * posterize_levels) / posterize_levels;
		result.b = floor(result.b * posterize_levels) / posterize_levels;
	}

	result = applyArtisticEffects(result, vs_FragPos, vs_barycentric, time);
	FragColor = vec4(result, 1.0);
}
