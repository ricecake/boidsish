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
uniform sampler2D reactionDiffusionTexture;

void main() {
	// discard;
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

	// --- Reaction Diffusion Sampling ---
	vec2 rd_coords = WorldPos.xz / 600.0 + 0.5;
	vec2 rd_val = texture(reactionDiffusionTexture, rd_coords).rg;
	vec3 rd_color = mix(vec3(0.1, 0.1, 0.2), vec3(0.8, 0.9, 1.0), rd_val.x);

	// --- Combine colors ---
	float reflection_strength = 0.8;
	vec3 lit_surface = lighting * surfaceColor;
	vec3 reflected_surface = mix(lit_surface, reflectionColor, reflection_strength);
	vec3 rd_surface = mix(reflected_surface, rd_color, 0.8);
	vec3 final_color = rd_surface + grid_color;

	// --- Distance Fade ---
	float dist = length(WorldPos.xz - viewPos.xz);
	float fade_start = 550.0;
	float fade_end = 560.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist);

	vec4 outColor = vec4(final_color, fade);
	FragColor = mix(vec4(0.7, 0.1, 0.7, fade) * length(outColor), outColor, step(1, fade));
}
