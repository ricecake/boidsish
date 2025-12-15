#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

void main() {
	if (FragPos.y < 0.001) {
		discard;
	}

	vec3 objectColor = vec3(0, 0.3, 0.4); // A deep blue
	// vec3 objectColor = vec3(mix(0.2, 0.4, step(0.1, fract(FragPos.y))), 0, 0.5); // A deep blue

	// Ambient
	float ambientStrength = 0.2;
	vec3  ambient = ambientStrength * lightColor;

	// Diffuse
	vec3  norm = normalize(Normal);
	vec3  lightDir = normalize(lightPos - FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3  diffuse = diff * lightColor;

	// Specular
	float specularStrength = 0.8;
	vec3  viewDir = normalize(viewPos - FragPos);
	vec3  reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
	vec3  specular = specularStrength * spec * lightColor;

	// --- Grid logic ---
	float grid_spacing = 1.0;
	vec2  coord = FragPos.xz / grid_spacing;
	vec2  f = fwidth(coord);

	vec2  grid_minor = abs(fract(coord - 0.5) - 0.5) / f;
	float line_minor = min(grid_minor.x, grid_minor.y);
	float C_minor = 1.0 - min(line_minor, 1.0);

	vec2  grid_major = abs(fract(coord / 5.0 - 0.5) - 0.5) / f;
	float line_major = min(grid_major.x, grid_major.y);
	float C_major = 1.0 - min(line_major, 1.0);

	float intensity = max(C_minor, C_major * 1.5) * 0.6;
	vec3  grid_color = vec3(0.0, 0.8, 0.8) * intensity;

	vec3 result = ((ambient + diffuse) * objectColor + specular) + grid_color; // Add specular on top
	// --- Distance Fade ---
	float dist = length(FragPos.xz);
	float fade_start = 450.0;
	float fade_end = 500.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

	FragColor = vec4(result, fade);
}
