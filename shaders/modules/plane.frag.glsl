//- Fragment shader logic for the ground plane
void fragment_plane() {
	// --- Reflection sampling ---
	vec2 texCoords = vs_ReflectionClipSpacePos.xy / vs_ReflectionClipSpacePos.w / 2.0 + 0.5;
	vec3 reflectionColor = texture(reflectionTexture, texCoords).rgb;

	// --- Grid logic ---
	float grid_spacing = 1.0;
	vec2  coord = vs_WorldPos.xz / grid_spacing;
	vec2  f = fwidth(coord);

	vec2  grid_minor = abs(fract(coord - 0.5) - 0.5) / f;
	float line_minor = min(grid_minor.x, grid_minor.y);
	float C_minor = 1.0 - min(line_minor, 1.0);

	vec2  grid_major = abs(fract(coord / 5.0 - 0.5) - 0.5) / f;
	float line_major = min(grid_major.x, grid_major.y);
	float C_major = 1.0 - min(line_major, 1.0);

	float intensity = max(C_minor, C_major * 1.5) * 0.6;
	vec3  grid_color = vec3(0.0, 0.8, 0.8) * intensity;

	// --- Plane lighting ---
	float ambientStrength = 0.05;
	vec3  ambient = ambientStrength * lightColor;

	vec3  norm = normalize(vs_Normal);
	vec3  lightDir = normalize(lightPos - vs_WorldPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3  diffuse = diff * lightColor;

	float specularStrength = 0.8;
	vec3  viewDir = normalize(viewPos - vs_WorldPos);
	vec3  reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3  specular = specularStrength * spec * lightColor;

	vec3 surfaceColor = vec3(0.05, 0.05, 0.08);
	vec3 lighting = (ambient + diffuse + specular);

	// --- Combine colors ---
	float reflection_strength = 0.8;
	vec3  final_color = mix(lighting * surfaceColor, reflectionColor, reflection_strength) + grid_color;

	// --- Distance Fade ---
	float dist = length(vs_WorldPos.xz - viewPos.xz);
	float fade_start = 550.0;
	float fade_end = 560.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

	vec4 outColor = vec4(final_color, fade);
	FragColor = mix(vec4(0.7, 0.1, 0.7, fade) * length(outColor), outColor, step(1, fade));
}
