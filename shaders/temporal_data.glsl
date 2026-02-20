layout(std140, binding = 6) uniform TemporalData {
	mat4  currentViewProjection;
	mat4  prevViewProjection;
	mat4  uProjection;
	mat4  invProjection;
	mat4  invView;
	vec2  texelSize;
	int   frameIndex;
	float padding;
};
