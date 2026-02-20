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

uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform bool is_instanced = false;
uniform bool useSSBOInstancing = false;

void main() {
	mat4 modelMatrix;
	if (useSSBOInstancing) {
		modelMatrix = ssboInstances[gl_InstanceID].model;
	} else if (is_instanced) {
		modelMatrix = aInstanceMatrix;
	} else {
		modelMatrix = model;
	}
	gl_Position = lightSpaceMatrix * modelMatrix * vec4(aPos, 1.0);
}
