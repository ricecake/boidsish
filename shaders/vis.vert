#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in mat4 aInstanceMatrix;
layout(location = 7) in vec4 aInstanceColor;

struct SSBOInstance {
	mat4 model;
	mat4 invModel;
};

// SSBO for decor/foliage instancing (binding 10)
layout(std430, binding = 10) buffer SSBOInstances {
	SSBOInstance ssboInstances[];
};

#include "frustum.glsl"
#include "lighting.glsl"
#include "helpers/shockwave.glsl"
#include "temporal_data.glsl"
#include "visual_effects.glsl"
#include "visual_effects.vert"

out vec3 FragPos;
out vec3 LocalPos;
out mat4 InvModelMatrix;
out vec4 CurPosition;
out vec4 PrevPosition;
out vec3 Normal;
out vec3 vs_color;
out vec3 barycentric;
out vec2 TexCoords;
out vec4 InstanceColor;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;
uniform float ripple_strength;
uniform bool  isColossal = false;
uniform bool  is_instanced = false;
uniform bool  useInstanceColor = false;
uniform bool  useSSBOInstancing = false;
uniform bool  isLine = false;
uniform bool  enableFrustumCulling = false;
uniform float frustumCullRadius = 5.0; // Approximate object radius for sphere test
uniform vec3  u_localCenter = vec3(0.0);

// Arcade Text Effects
uniform bool  isArcadeText = false;
uniform int   arcadeWaveMode = 0; // 0: None, 1: Vertical, 2: Flag, 3: Twist
uniform float arcadeWaveAmplitude = 0.5;
uniform float arcadeWaveFrequency = 10.0;
uniform float arcadeWaveSpeed = 5.0;

void main() {
	vec3 displacedPos = aPos;
	vec3 displacedNormal = aNormal;

	if (isArcadeText) {
		float x = aTexCoords.x;
		float phase = time * arcadeWaveSpeed;

		if (arcadeWaveMode == 1) { // Vertical Rippling Wave
			displacedPos.y += sin(x * arcadeWaveFrequency + phase) * arcadeWaveAmplitude;
		} else if (arcadeWaveMode == 2) { // Flag Style Wave
			displacedPos.y += sin(x * arcadeWaveFrequency + phase) * arcadeWaveAmplitude;
			displacedPos.z += cos(x * arcadeWaveFrequency * 0.7 + phase * 0.8) * arcadeWaveAmplitude * 0.5;
		} else if (arcadeWaveMode == 3) { // Lengthwise Twist
			float twist_angle = (x - 0.5) * arcadeWaveAmplitude * sin(phase);
			float s = sin(twist_angle);
			float c = cos(twist_angle);
			// Rotate around local X axis
			float y = displacedPos.y;
			float z = displacedPos.z;
			displacedPos.y = y * c - z * s;
			displacedPos.z = y * s + z * c;
		}
	}

	if (glitched_enabled == 1) {
		displacedPos = applyGlitch(displacedPos, time);
	}

	if (ripple_strength > 0.0) {
		float frequency = 20.0;
		float speed = 3.0;
		float amplitude = ripple_strength;

		float wave = sin(frequency * (aPos.x + aPos.z) + time * speed);
		displacedPos = aPos + aNormal * wave * amplitude;

		vec3 gradient = vec3(
			cos(frequency * (aPos.x + aPos.z) + time * speed) * frequency * amplitude,
			0.0,
			cos(frequency * (aPos.x + aPos.z) + time * speed) * frequency * amplitude
		);
		displacedNormal = normalize(aNormal - gradient);
	}

	mat4 modelMatrix;
	mat4 invModelMatrix_local;
	if (useSSBOInstancing) {
		modelMatrix = ssboInstances[gl_InstanceID].model;
		invModelMatrix_local = ssboInstances[gl_InstanceID].invModel;
	} else if (is_instanced) {
		modelMatrix = aInstanceMatrix;
	} else {
		modelMatrix = model;
		invModelMatrix_local = inverse(modelMatrix);
	}

	// Transform local center to world space for accurate frustum culling
	vec3 instanceCenter = vec3(modelMatrix * vec4(u_localCenter, 1.0));

	// Extract maximum scale factor across all axes
	float instanceScale = max(length(vec3(modelMatrix[0])),
	                          max(length(vec3(modelMatrix[1])), length(vec3(modelMatrix[2]))));

	// GPU frustum culling - output degenerate triangle if outside frustum
	if (enableFrustumCulling && !isColossal) {
		// Use sphere test with approximate radius based on scale
		float effectiveRadius = frustumCullRadius * instanceScale;

		if (!isSphereInFrustum(instanceCenter, effectiveRadius)) {
			// Output degenerate triangle (all vertices at same point)
			// GPU will automatically cull this
			gl_Position = vec4(0.0, 0.0, -2.0, 1.0); // Behind near plane
			FragPos = vec3(0.0);
			Normal = vec3(0.0, 1.0, 0.0);
			TexCoords = vec2(0.0);
			InstanceColor = vec4(0.0);
			gl_ClipDistance[0] = -1.0; // Clip it
			return;
		}
	}

	LocalPos = displacedPos;
	InvModelMatrix = invModelMatrix_local;
	FragPos = vec3(modelMatrix * vec4(displacedPos, 1.0));

	// Apply shockwave displacement (sway for decor)
	// Calculate at instanceCenter to prevent warping, scale by world-relative height
	if (useSSBOInstancing) {
		FragPos += getShockwaveDisplacement(instanceCenter, aPos.y * instanceScale, true);
	}

	Normal = mat3(transpose(inverse(modelMatrix))) * displacedNormal;
	TexCoords = aTexCoords;
	InstanceColor = useInstanceColor ? aInstanceColor : vec4(1.0);
	if (wireframe_enabled == 1) {
		barycentric = getBarycentric();
	}

	if (isColossal) {
		mat4 staticView = mat4(mat3(view));
		vec3 skyPositionOffset = vec3(0.0, -10.0, -500.0);
		vec4 world_pos = model * vec4(displacedPos * 50, 1.0);
		world_pos.xyz += skyPositionOffset;
		gl_Position = projection * staticView * world_pos;
		gl_Position.z = gl_Position.w * 0.99999;
		FragPos = world_pos.xyz;

		CurPosition = gl_Position;
		PrevPosition =
			gl_Position; // Sky box doesn't move relative to camera normally, or we don't care about its velocity
	} else {
		gl_Position = projection * view * vec4(FragPos, 1.0);
		CurPosition = gl_Position;
		// Since we don't have previous model matrix, we assume static objects for velocity
		// This is sufficient for GTAO and most environment reprojection.
		PrevPosition = prevViewProjection * vec4(FragPos, 1.0);
	}

	gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
}
