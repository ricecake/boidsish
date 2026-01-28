#version 430 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 Color;

out vec4 FragColor;

uniform vec3 u_camera_pos;

void main() {
	if (Color.a <= 0.0)
		discard;

	vec3  norm = normalize(Normal);
	vec3  lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Simple directional light
	float diff = max(dot(norm, lightDir), 0.3);

	FragColor = vec4(Color.rgb * diff, Color.a);
}
