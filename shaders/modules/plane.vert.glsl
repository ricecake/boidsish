//- Vertex shader logic for the ground plane
void vertex_plane() {
	vs_WorldPos = vec3(model * vec4(aPos, 1.0));
	vs_WorldPos.xz += viewPos.xz;
	vs_Normal = vec3(0.0, 1.0, 0.0);
	vs_ReflectionClipSpacePos = reflectionViewProjection * vec4(vs_WorldPos, 1.0);
	gl_Position = projection * view * vec4(vs_WorldPos, 1.0);
}
