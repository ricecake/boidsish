#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec4 ReflectionClipSpacePos;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

uniform sampler2D reflectionTexture;

void main() {
	// --- Reflection sampling ---
	vec2 texCoords = ReflectionClipSpacePos.xy / ReflectionClipSpacePos.w / 2.0 + 0.5;
	vec3 reflectionColor = texture(reflectionTexture, texCoords).rgb;

	// --- Grid logic ---
	float grid_spacing = 1.0;
	vec2  coord = WorldPos.xz / grid_spacing;
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

	vec3  norm = normalize(Normal);
	vec3  lightDir = normalize(lightPos - WorldPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3  diffuse = diff * lightColor;

	float specularStrength = 0.8;
	vec3  viewDir = normalize(viewPos - WorldPos);
	vec3  reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3  specular = specularStrength * spec * lightColor;

	vec3 surfaceColor = vec3(0.05, 0.05, 0.08);
	vec3 lighting = (ambient + diffuse + specular);

	// --- Combine colors ---
	float reflection_strength = 0.8;
	vec3  final_color = mix(lighting * surfaceColor, reflectionColor, reflection_strength) + grid_color;

	// --- Distance Fade ---
	float dist = length(WorldPos.xz - viewPos.xz);
	float fade_start = 350.0;
	float fade_end = 425.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

	FragColor = vec4(final_color, fade);
	// FragColor = vec4(0,0,0,1);
}
