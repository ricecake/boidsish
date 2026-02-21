#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 3) in mat4 aInstanceMatrix;

struct SSBOInstance {
	mat4 model;
	mat4 invModel;
};

// SSBO for decor/foliage instancing (binding 10)
layout(std430, binding = 10) buffer SSBOInstances {
	SSBOInstance ssboInstances[];
};

uniform mat4  lightSpaceMatrix;
uniform mat4  model;
uniform bool  is_instanced = false;
uniform bool  useSSBOInstancing = false;
uniform vec3  u_localCenter = vec3(0.0);
uniform float frustumCullRadius = 5.0;

#include "frustum.glsl"

void main() {
	mat4 modelMatrix;
	if (useSSBOInstancing) {
		modelMatrix = ssboInstances[gl_InstanceID].model;
	} else if (is_instanced) {
		modelMatrix = aInstanceMatrix;
	} else {
		modelMatrix = model;
	}

	// Simple frustum culling for shadow pass to avoid unnecessary geometry processing
	vec3  worldCenter = vec3(modelMatrix * vec4(u_localCenter, 1.0));
	float instanceScale = max(length(vec3(modelMatrix[0])),
	                          max(length(vec3(modelMatrix[1])), length(vec3(modelMatrix[2]))));
	float effectiveRadius = frustumCullRadius * instanceScale;

	// Note: We'd need to pass the shadow frustum here. For now, we skip culling in shadow pass
	// or assume the main frustum is large enough. Actually, shadow pass usually doesn't cull
	// unless we have the shadow frustum.

	gl_Position = lightSpaceMatrix * modelMatrix * vec4(aPos, 1.0);
}
