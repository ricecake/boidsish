#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 vs_color;
in vec4 ClipPos;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

uniform vec3  objectColor;
uniform int   useVertexColor;
uniform bool  colorShift;
uniform float chromaticAberrationStrength = 0.01;

void main() {
	// Ambient
	float ambientStrength = 0.1;
	vec3  ambient = ambientStrength * lightColor;

	// Diffuse
	vec3  norm = normalize(Normal);
	vec3  lightDir = normalize(lightPos - FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3  diffuse = diff * lightColor;

	// Specular
	float specularStrength = 1.0;
	vec3  viewDir = normalize(viewPos - FragPos);
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
		// Screen-space chromatic aberration using derivatives.
		// This approximates sampling at offset screen-space coordinates
		// without needing a full post-processing pass.
		vec2 ndc = ClipPos.xy / ClipPos.w;
		vec2 offset = ndc * chromaticAberrationStrength;

		vec3 color = result;
		vec3 color_r = result + dFdx(result) * offset.x + dFdy(result) * offset.y;
		vec3 color_b = result - dFdx(result) * offset.x - dFdy(result) * offset.y;
		result = vec3(color_r.r, color.g, color_b.b);
	}

	FragColor = vec4(result, 1.0);
}
