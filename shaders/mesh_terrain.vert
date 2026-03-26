#version 420 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aBiome;

out vec3 FragPos;
out vec3 Normal;
out vec2 Biome;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 clipPlane;
uniform bool uUseMeshMode = false; // Declared for parity even if always true when this shader is used

void main() {
    // aPos is local to chunk [0, chunk_size], model includes chunk world offset
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;

    // Using transpose of inverse model matrix for normals if non-uniform scaling is used,
    // but here model is just translation, so we can use it directly or just use aNormal.
    Normal = mat3(model) * aNormal;
    Biome = aBiome;

	if (length(clipPlane.xyz) > 0.0001) {
		gl_ClipDistance[0] = dot(worldPos, clipPlane);
	} else {
		gl_ClipDistance[0] = 1.0;
	}

    gl_Position = projection * view * worldPos;
}
