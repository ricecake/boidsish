#version 330 core
layout(location = 0) in vec3 aPos;

out vec3 WorldPos;
out vec3 Normal;
out vec4 ReflectionClipSpacePos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 reflectionViewProjection;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

void main() {
	// Make the plane "infinite" by having it follow the camera
	vec3 pos = aPos;
	pos.xz += viewPos.xz;
	vec4 world_pos_4 = model * vec4(pos, 1.0);
	world_pos_4.y -= 2.0;
	WorldPos = world_pos_4.xyz;

	// The normal is always up for a flat plane
	Normal = vec3(0.0, 1.0, 0.0);

	// Calculate the clip space coordinates for the reflection texture
	ReflectionClipSpacePos = reflectionViewProjection * world_pos_4;

	// Final vertex position
	gl_Position = projection * view * world_pos_4;
}
