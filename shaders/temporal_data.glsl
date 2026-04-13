layout(std140, binding = [[TEMPORAL_DATA_BINDING]]) uniform TemporalData {
	mat4  viewProjection;
	mat4  prevViewProjection;
	mat4  uProjection;
	mat4  invProjection;
	mat4  invView;
	vec2  texelSize;
	int   frameIndex;
	float padding_temporal;
};
