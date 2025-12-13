#version 330 core
layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 10) out;

in vec3  vs_color[];
in float vs_progress[];

out vec3  color;
out float fade;
out vec3  normal;
out vec3  frag_pos; // Position in world space

uniform mat4  projection;
uniform mat4  view;
uniform float thickness;

// Calculates a robust, orthogonal frame (tangent, right, up)
void GetOrthonormalFrame(vec3 p_prev, vec3 p_curr, vec3 p_next, out vec3 tangent, out vec3 right, out vec3 up) {
	tangent = normalize(p_next - p_prev);

	vec3 world_up = vec3(0.0, 1.0, 0.0);
	if (abs(dot(tangent, world_up)) > 0.99) {
		world_up = vec3(1.0, 0.0, 0.0); // Avoid gimbal lock
	}

	right = normalize(cross(tangent, world_up));
	up = normalize(cross(right, tangent));
}

void main() {
	// Input points for the current segment
	vec3 p0 = gl_in[0].gl_Position.xyz;
	vec3 p1 = gl_in[1].gl_Position.xyz;
	vec3 p2 = gl_in[2].gl_Position.xyz;
	vec3 p3 = gl_in[3].gl_Position.xyz;

	// Attributes for the start and end of the segment
	vec3  c1 = vs_color[1];
	vec3  c2 = vs_color[2];
	float prog1 = vs_progress[1];
	float prog2 = vs_progress[2];

	// Calculate orthonormal frames for the start and end points
	vec3 tangent1, right1, up1;
	vec3 tangent2, right2, up2;
	GetOrthonormalFrame(p0, p1, p2, tangent1, right1, up1);
	GetOrthonormalFrame(p1, p2, p3, tangent2, right2, up2);

	// Build the 4 corner vertices and their normals for the start point (p1)
	vec3 p1_br = p1 + (right1 * thickness - up1 * thickness);
	vec3 n1_br = normalize(-up1 + right1);
	vec3 p1_bl = p1 + (-right1 * thickness - up1 * thickness);
	vec3 n1_bl = normalize(-up1 - right1);
	vec3 p1_tr = p1 + (right1 * thickness + up1 * thickness);
	vec3 n1_tr = normalize(up1 + right1);
	vec3 p1_tl = p1 + (-right1 * thickness + up1 * thickness);
	vec3 n1_tl = normalize(up1 - right1);

	// Build the 4 corner vertices and their normals for the end point (p2)
	vec3 p2_br = p2 + (right2 * thickness - up2 * thickness);
	vec3 n2_br = normalize(-up2 + right2);
	vec3 p2_bl = p2 + (-right2 * thickness - up2 * thickness);
	vec3 n2_bl = normalize(-up2 - right2);
	vec3 p2_tr = p2 + (right2 * thickness + up2 * thickness);
	vec3 n2_tr = normalize(up2 + right2);
	vec3 p2_tl = p2 + (-right2 * thickness + up2 * thickness);
	vec3 n2_tl = normalize(up2 - right2);

	// Emit a triangle strip that connects the start and end quads
	// The strip wraps around the tube: bottom -> right -> top -> left -> bottom

	// Bottom face
	frag_pos = p1_br;
	normal = n1_br;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c1;
	fade = prog1;
	EmitVertex();
	frag_pos = p2_br;
	normal = n2_br;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c2;
	fade = prog2;
	EmitVertex();

	// Right face
	frag_pos = p1_tr;
	normal = n1_tr;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c1;
	fade = prog1;
	EmitVertex();
	frag_pos = p2_tr;
	normal = n2_tr;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c2;
	fade = prog2;
	EmitVertex();

	// Top face
	frag_pos = p1_tl;
	normal = n1_tl;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c1;
	fade = prog1;
	EmitVertex();
	frag_pos = p2_tl;
	normal = n2_tl;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c2;
	fade = prog2;
	EmitVertex();

	// Left face
	frag_pos = p1_bl;
	normal = n1_bl;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c1;
	fade = prog1;
	EmitVertex();
	frag_pos = p2_bl;
	normal = n2_bl;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c2;
	fade = prog2;
	EmitVertex();

	// Re-emit first two vertices to close the loop back to the bottom face
	frag_pos = p1_br;
	normal = n1_br;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c1;
	fade = prog1;
	EmitVertex();
	frag_pos = p2_br;
	normal = n2_br;
	gl_Position = projection * view * vec4(frag_pos, 1.0);
	color = c2;
	fade = prog2;
	EmitVertex();

	EndPrimitive();
}
