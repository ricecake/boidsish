#ifndef TEMPORAL_DATA_GLSL
#define TEMPORAL_DATA_GLSL

layout(std140, binding = 6) uniform TemporalData {
	mat4  viewProjection;
	mat4  prevViewProjection;
	mat4  uProjection;
	mat4  invProjection;
	mat4  invView;
	vec3  viewPos;
	float padding_pos;
	vec2  texelSize;
	int   frameIndex;
	float nearPlane;
	float farPlane;
	float padding_temporal[2];
} td;

#endif
