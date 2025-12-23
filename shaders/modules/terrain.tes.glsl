// Helper: Projects point q onto the plane defined by vertex v and normal n
vec3 projectPointOnPlane_terrain(vec3 q, vec3 v, vec3 n) {
	return q - dot(q - v, n) * n;
}

// Helper: Bilinear interpolation for vec3
vec3 bilerp_terrain(vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec2 uv) {
	vec3 bot = mix(v0, v1, uv.x);
	vec3 top = mix(v3, v2, uv.x);
	return mix(bot, top, uv.y);
}

//- Tessellation evaluation shader logic for terrain
void tess_eval_terrain() {
	vec2 uv = gl_TessCoord.xy;

	vec2 texCoord1 = mix(vs_TexCoords_TC_out[0], vs_TexCoords_TC_out[1], uv.x);
	vec2 texCoord2 = mix(vs_TexCoords_TC_out[3], vs_TexCoords_TC_out[2], uv.x);
	vs_TexCoords = mix(texCoord1, texCoord2, uv.y);

	vec3 pos1 = mix(vs_WorldPos_TC_out[0], vs_WorldPos_TC_out[1], uv.x);
	vec3 pos2 = mix(vs_WorldPos_TC_out[3], vs_WorldPos_TC_out[2], uv.x);
	vec3 q = mix(pos1, pos2, uv.y);

	vec3 norm1 = mix(vs_Normal_TC_out[0], vs_Normal_TC_out[1], uv.x);
	vec3 norm2 = mix(vs_Normal_TC_out[3], vs_Normal_TC_out[2], uv.x);
	vs_Normal = normalize(mix(norm1, norm2, uv.y));

	vec3 p0 = projectPointOnPlane_terrain(q, vs_WorldPos_TC_out[0], vs_Normal_TC_out[0]);
	vec3 p1 = projectPointOnPlane_terrain(q, vs_WorldPos_TC_out[1], vs_Normal_TC_out[1]);
	vec3 p2 = projectPointOnPlane_terrain(q, vs_WorldPos_TC_out[2], vs_Normal_TC_out[2]);
	vec3 p3 = projectPointOnPlane_terrain(q, vs_WorldPos_TC_out[3], vs_Normal_TC_out[3]);

	vec3 posCurved = bilerp_terrain(p0, p1, p2, p3, uv);
	vs_FragPos = mix(q, posCurved, uPhongAlpha);

	gl_Position = projection * view * vec4(vs_FragPos, 1.0);
}
