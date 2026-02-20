#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 3) in mat4 aInstanceMatrix;

struct SSBOInstance {
	mat4 model;
	mat4 invModel;
};

layout(std430, binding = 10) buffer SSBOInstances {
	SSBOInstance ssboInstances[];
};

uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform mat4 invModel;
uniform bool is_instanced = false;
uniform bool useSSBOInstancing = false;

uniform mat4 view;
uniform mat4 projection;

uniform vec3 u_sdfExtent;
uniform vec3 u_sdfMin;
uniform float u_sdfShadowMaxDist = 2.0;
uniform vec3 u_worldLightDir;

out mat4  v_invModel;

void main() {
	mat4 modelMatrix;
	mat4 invModelMatrix;

	if (useSSBOInstancing) {
		modelMatrix = ssboInstances[gl_InstanceID].model;
		invModelMatrix = ssboInstances[gl_InstanceID].invModel;
	} else if (is_instanced) {
		modelMatrix = aInstanceMatrix;
		invModelMatrix = inverse(aInstanceMatrix);
	} else {
		modelMatrix = model;
		invModelMatrix = invModel;
	}

	// Transform local box to cover the SDF volume
    // aPos is [-0.5, 0.5]
    vec3 localPos = u_sdfMin + (aPos + 0.5) * u_sdfExtent;

    // Simple expansion of the bounding box to cover potential shadows
    // This is a very conservative approach: just expand in all directions
    localPos += aPos * u_sdfShadowMaxDist;

    // Pass invModel to fragment shader
    v_invModel = invModelMatrix;

	gl_Position = projection * view * modelMatrix * vec4(localPos, 1.0);
}
