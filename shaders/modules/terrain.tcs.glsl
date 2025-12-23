float GetVertexTessLevel_terrain(vec3 worldPos, vec3 normal, vec3 forward) {
    float dist = distance(viewPos, worldPos);
    float distanceLevel = uTessLevelMax / (dist / 8.0);
    distanceLevel = clamp(distanceLevel, uTessLevelMin, uTessLevelMax);

    vec3  viewDir = normalize(viewPos - worldPos);
    float dotProd = dot(normalize(normal), viewDir);
    float isSilhouette = 1.0 - abs(dotProd);
    float silhouetteBoost = mix(1.0, 3.0, isSilhouette);

    vec3 dirCamToVertex = normalize(worldPos - viewPos);
    float align = dot(dirCamToVertex, forward);
    float focusCull = smoothstep(0.8, 1.0, align);

    float finalLevel = distanceLevel * silhouetteBoost;
    finalLevel *= focusCull;

    return step(0, finalLevel) * max(finalLevel, uTessLevelMin);
}

//- Tessellation control shader logic for terrain
void tess_control_terrain() {
	vs_WorldPos_TC_out[gl_InvocationID] = vs_WorldPos_VS_out[gl_InvocationID];
	vs_TexCoords_TC_out[gl_InvocationID] = vs_TexCoords_VS_out[gl_InvocationID];
	vs_Normal_TC_out[gl_InvocationID] = vs_Normal_VS_out[gl_InvocationID];

	if (gl_InvocationID == 0) {
		float l0 = GetVertexTessLevel_terrain(vs_WorldPos_VS_out[0], vs_Normal_VS_out[0], vs_viewForward[0]);
		float l1 = GetVertexTessLevel_terrain(vs_WorldPos_VS_out[1], vs_Normal_VS_out[1], vs_viewForward[1]);
		float l2 = GetVertexTessLevel_terrain(vs_WorldPos_VS_out[2], vs_Normal_VS_out[2], vs_viewForward[2]);
		float l3 = GetVertexTessLevel_terrain(vs_WorldPos_VS_out[3], vs_Normal_VS_out[3], vs_viewForward[3]);

		float edge0 = (l0 + l1) * 0.5;
		float edge1 = (l1 + l2) * 0.5;
		float edge2 = (l2 + l3) * 0.5;
		float edge3 = (l3 + l0) * 0.5;

		gl_TessLevelOuter[0] = clamp(edge0, uTessLevelMin, uTessLevelMax);
		gl_TessLevelOuter[1] = clamp(edge1, uTessLevelMin, uTessLevelMax);
		gl_TessLevelOuter[2] = clamp(edge2, uTessLevelMin, uTessLevelMax);
		gl_TessLevelOuter[3] = clamp(edge3, uTessLevelMin, uTessLevelMax);

		gl_TessLevelInner[0] = max(gl_TessLevelOuter[0], gl_TessLevelOuter[2]);
		gl_TessLevelInner[1] = max(gl_TessLevelOuter[1], gl_TessLevelOuter[3]);
	}
}
