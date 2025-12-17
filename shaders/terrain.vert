#version 420 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

out vec3 WorldPos_VS_out;
out vec2 TexCoords_VS_out;
out vec3 Normal_VS_out;

uniform mat4 model;

layout(std140, binding = 0) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

void main() {
    // gl_Position = model * vec4(aPos, 1.0);
	gl_Position = vec4(aPos, 1.0);
	WorldPos_VS_out = vec3(model * vec4(aPos, 1.0));
	TexCoords_VS_out = aTexCoords;
	Normal_VS_out = mat3(transpose(inverse(model))) * aNormal;

	// vec3  viewDir = viewPos - WorldPos_VS_out;
	// gl_Position *= abs(sin(time));
	// WorldPos_VS_out *= abs(dot(viewDir, WorldPos_VS_out) * sin(time/3));
}
