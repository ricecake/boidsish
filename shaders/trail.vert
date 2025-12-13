#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in float aProgress;

out vec3  vs_color;
out float vs_progress;
out vec3  vs_normal;
out vec3  vs_frag_pos;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;
uniform float base_thickness;

void main() {
	// Calculate tapering scale based on progress
	// Progress 0 = oldest (tail), Progress 1 = newest (head)
	// We want thick at head (newest) and thin at tail (oldest)
	float taper_scale = 0.2 + 0.8 * aProgress;

	// Calculate the displacement required to scale the tube's radius
	// This moves the vertex closer to or further from the tube's spine
	vec3 offset = aNormal * base_thickness * (taper_scale - 1.0);
	vec3 final_pos = aPos + offset;

	vs_color = aColor;
	vs_progress = aProgress; // Pass progress along, might be useful later
	vs_normal = mat3(transpose(inverse(model))) * aNormal;
	vs_frag_pos = vec3(model * vec4(final_pos, 1.0));

	vec4 world_pos = model * vec4(final_pos, 1.0);
	gl_ClipDistance[0] = dot(world_pos, clipPlane);
	gl_Position = projection * view * world_pos;
}
