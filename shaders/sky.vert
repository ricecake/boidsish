#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 viewDirection;

uniform mat4 invProjection;
uniform mat4 invView;

void main()
{
	// This creates a fullscreen triangle
	vec2 pos = vec2(-1.0, -1.0);
	if(gl_VertexID == 1) pos = vec2(3.0, -1.0);
	else if(gl_VertexID == 2) pos = vec2(-1.0, 3.0);

	gl_Position = vec4(pos, 1.0, 1.0);

	vec4 unprojected = invProjection * gl_Position;
	unprojected /= unprojected.w;

	// For moon/nebula, needs to be relative to camera
	vec4 world_coords = invView * vec4(unprojected.xy, -1.0, 1.0);
	viewDirection = world_coords.xyz - inverse(invView)[3].xyz;
}
