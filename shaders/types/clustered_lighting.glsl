#ifndef CLUSTERED_LIGHTING_TYPES_GLSL
#define CLUSTERED_LIGHTING_TYPES_GLSL

struct Cluster {
	uint count;
	uint lightIndices[64];
	uint padding[3];
};

layout(std430, binding = [[LIGHTS_BUFFER_BINDING]]) readonly buffer LightsBuffer {
	Light lights[];
};

layout(std430, binding = [[CLUSTER_GRID_BINDING]]) buffer ClusterGridBuffer {
	Cluster clusters[];
};

#endif
