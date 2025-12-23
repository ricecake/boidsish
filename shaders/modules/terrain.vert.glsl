//- Vertex shader logic for terrain
void vertex_terrain() {
	vs_viewForward = vec3(-view[0][2], -view[1][2], -view[2][2]);
	gl_Position = vec4(aPos, 1.0);
	vs_WorldPos_VS_out = vec3(model * vec4(aPos, 1.0));
	vs_TexCoords_VS_out = aTexCoords;
	vs_Normal_VS_out = mat3(transpose(inverse(model))) * aNormal;
}
