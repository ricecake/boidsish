#version 430 core

in GS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 Color;
} fs_in;

out vec4 FragColor;

uniform vec3 u_camera_pos;

void main() {
	if (fs_in.Color.a <= 0.0)
		discard;

	vec3  norm = normalize(fs_in.Normal);
	vec3  lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Simple directional light
	float diff = max(dot(norm, lightDir), 0.3);

	FragColor = vec4(fs_in.Color.rgb * diff, fs_in.Color.a);
}
