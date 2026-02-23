#version 460 core
#extension GL_ARB_shader_draw_parameters : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 8) in vec3 aVertexColor;

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;

// SSBO for decor/foliage instancing (binding 10)
layout(std430, binding = 10) buffer SSBOInstances {
	mat4 ssboInstanceMatrices[];
};

#include "frustum.glsl"
#include "helpers/lighting.glsl"
#include "helpers/shockwave.glsl"
#include "temporal_data.glsl"
#include "visual_effects.glsl"
#include "visual_effects.vert"

out vec3     FragPos;
out vec4     CurPosition;
out vec4     PrevPosition;
out vec3     Normal;
out vec3     vs_color;
out vec3     barycentric;
out vec2     TexCoords;
out vec4     InstanceColor;
out float    WindDeflection;
flat out int vUniformIndex;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;
uniform float ripple_strength;
uniform bool  isColossal = false;
uniform bool  useSSBOInstancing = false;
uniform bool  isLine = false;
uniform bool  enableFrustumCulling = false;
uniform float frustumCullRadius = 5.0; // Approximate object radius for sphere test

uniform vec3  u_aabbMin;
uniform vec3  u_aabbMax;
uniform float u_windResponsiveness;

// Arcade Text Effects
uniform bool  isArcadeText = false;
uniform int   arcadeWaveMode = 0; // 0: None, 1: Vertical, 2: Flag, 3: Twist
uniform float arcadeWaveAmplitude = 0.5;
uniform float arcadeWaveFrequency = 10.0;
uniform float arcadeWaveSpeed = 5.0;

void main() {
#ifdef GL_ARB_shader_draw_parameters
	int drawID = gl_DrawIDARB;
#else
	int drawID = gl_DrawID;
#endif

	vUniformIndex = uUseMDI ? drawID : -1;
	bool use_ssbo = uUseMDI && vUniformIndex >= 0;

	mat4  current_model = use_ssbo ? uniforms_data[vUniformIndex].model : model;
	bool  current_isArcadeText = use_ssbo ? (uniforms_data[vUniformIndex].is_arcade_text != 0) : isArcadeText;
	int   current_arcadeWaveMode = use_ssbo ? uniforms_data[vUniformIndex].arcade_wave_mode : arcadeWaveMode;
	float current_arcadeWaveAmplitude = use_ssbo ? uniforms_data[vUniformIndex].arcade_wave_amplitude
												 : arcadeWaveAmplitude;
	float current_arcadeWaveFrequency = use_ssbo ? uniforms_data[vUniformIndex].arcade_wave_frequency
												 : arcadeWaveFrequency;
	float current_arcadeWaveSpeed = use_ssbo ? uniforms_data[vUniformIndex].arcade_wave_speed : arcadeWaveSpeed;
	bool  current_isColossal = use_ssbo ? (uniforms_data[vUniformIndex].is_colossal != 0) : isColossal;
	bool  current_useSSBOInstancing = use_ssbo ? (uniforms_data[vUniformIndex].use_ssbo_instancing != 0)
											   : useSSBOInstancing;

	WindDeflection = 0.0;
	vec3 displacedPos = aPos;
	vec3 displacedNormal = aNormal;

	if (current_isArcadeText) {
		float x = aTexCoords.x;
		float phase = time * current_arcadeWaveSpeed;

		if (current_arcadeWaveMode == 1) { // Vertical Rippling Wave
			displacedPos.y += sin(x * current_arcadeWaveFrequency + phase) * current_arcadeWaveAmplitude;
		} else if (current_arcadeWaveMode == 2) { // Flag Style Wave
			displacedPos.y += sin(x * current_arcadeWaveFrequency + phase) * current_arcadeWaveAmplitude;
			displacedPos.z += cos(x * current_arcadeWaveFrequency * 0.7 + phase * 0.8) * current_arcadeWaveAmplitude *
				0.5;
		} else if (current_arcadeWaveMode == 3) { // Lengthwise Twist
			float twist_angle = (x - 0.5) * current_arcadeWaveAmplitude * sin(phase);
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
	if (current_useSSBOInstancing) {
		modelMatrix = ssboInstanceMatrices[gl_InstanceID];
	} else {
		modelMatrix = current_model;
	}

	// Extract world position (translation from model matrix)
	vec3  instanceCenter = vec3(modelMatrix[3]);
	float instanceScale = length(vec3(modelMatrix[0])); // Approximate scale from first column

	// GPU frustum culling - output degenerate triangle if outside frustum
	if (enableFrustumCulling && !current_isColossal) {
		// Use sphere test with approximate radius based on scale
		float effectiveRadius = frustumCullRadius * instanceScale;

		if (!isSphereInFrustum(instanceCenter, effectiveRadius)) {
			// Output degenerate triangle (all vertices at same point)
			// GPU will automatically cull this
			gl_Position = vec4(0.0, 0.0, -2.0, 1.0); // Behind near plane
			FragPos = vec3(0.0);
			Normal = vec3(0.0, 1.0, 0.0);
			TexCoords = vec2(0.0);
			gl_ClipDistance[0] = -1.0; // Clip it
			return;
		}
	}

	FragPos = vec3(modelMatrix * vec4(displacedPos, 1.0));

	// Apply shockwave displacement (sway for decor)
	// Calculate at instanceCenter to prevent warping, scale by world-relative height
	if (current_useSSBOInstancing) {
		FragPos += getShockwaveDisplacement(instanceCenter, (aPos.y - u_aabbMin.y) * instanceScale, true);

		// Apply wind sway
		if (wind_strength > 0.0) {
			float localHeight = max(0.0, aPos.y - u_aabbMin.y);
			float totalHeight = max(0.001, u_aabbMax.y - u_aabbMin.y);
			float normalizedHeight = clamp(localHeight / totalHeight, 0.0, 1.0);

			// Use instanceCenter for coherent wind across the whole object
			// We use a combination of world position and time for the noise seed
			vec2 windNudge = curlNoise2D(instanceCenter.xz * wind_frequency + time * wind_speed * 0.5) * wind_strength *
				u_windResponsiveness;
			WindDeflection = length(windNudge);

			// Scale nudge by height (bending effect)
			FragPos.xz += windNudge * pow(normalizedHeight, 1.2);
		}
	}

	Normal = mat3(transpose(inverse(modelMatrix))) * displacedNormal;
	TexCoords = aTexCoords;
	vs_color = aVertexColor;
	if (wireframe_enabled == 1) {
		barycentric = getBarycentric();
	}

	if (current_isColossal) {
		mat4 staticView = mat4(mat3(view));
		vec3 skyPositionOffset = vec3(0.0, -10.0, -500.0);
		vec4 world_pos = current_model * vec4(displacedPos * 50, 1.0);
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
