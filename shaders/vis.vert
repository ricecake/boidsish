#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

#include "visual_effects.glsl"
#include "visual_effects.vert"

out vec3 FragPos;
out vec3 Normal;
out vec3 vs_color;
out vec3 barycentric;
out vec2 TexCoords;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;
uniform float ripple_strength;
uniform bool  isColossal = true;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

void main() {
	vec3 displacedPos = aPos;
	vec3 displacedNormal = aNormal;

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

	FragPos = vec3(model * vec4(displacedPos, 1.0));
	Normal = mat3(transpose(inverse(model))) * displacedNormal;
	TexCoords = aTexCoords;
	if (wireframe_enabled == 1) {
		barycentric = getBarycentric();
	}

/*	if (isColossal) {
		// --- Colossal Object Logic (View Space) ---
		view = mat4(mat3(view));
		vec4 view_pos = view * vec4(FragPos, 1.0);
		view_pos.xy *= 500.0;
		view_pos.z = -500.0;
		// gl_Position = projection * view_pos;
gl_Position = projection * view * vec4(FragPos, 1.0);
		// --- Safer FragPos Calculation ---
		vec3 view_dir = normalize(view_pos.xyz);
		FragPos = viewPos + view_dir;

*/
// if (isColossal) {
// 		// 1. Create a View Matrix that removes translation
// 		//    We cast to mat3 to keep only rotation/scale, then cast back to mat4.
// 		//    This effectively anchors the object to the camera's position (0,0,0).
// 		mat4 staticView = mat4(mat3(view));

// 		// 2. Calculate position using this static view
// 		//    We still use 'model' so the object can animate/rotate itself.
// 		vec4 view_pos = staticView * model * vec4(displacedPos, 1.0);
// 		view_pos.xy *= 500.0;
// 		view_pos.z = -500.0;

// 		// 3. Project normally
// 		gl_Position = projection * view_pos;

// 		// 4. (Optional) Force Z to the far plane
// 		//    This ensures it renders behind everything else.
// 		//    We set z = w so that after perspective divide (z/w), depth = 1.0.
// 		gl_Position = gl_Position.xyww;
// 		// gl_Position.z = gl_Position.w - 0.001;
// 		// gl_Position.xy *= 100;

// 		// --- Lighting Correction ---
// 		// Since we tampered with the position, lighting calculations relying on
// 		// world-space distance (like attenuation) might break.
// 		// For a colossal object, directional light (sun) is usually best.
// 		// If you need it, you might reset FragPos to a very distant point:
// 		FragPos = vec3(model * vec4(displacedPos, 1.0));
// }

if (isColossal) {
		// 1. Strip translation from view (Standard Skybox Trick)
		mat4 staticView = mat4(mat3(view));

		// 2. Define a direction and distance for the object in WORLD space.
		//    If you leave it at 0,0,0, it sits on the camera lens.
		//    Here we push it 500 units along the -Z axis (North).
		//    ADJUST THIS vec3 TO PLACE THE OBJECT IN THE SKY WHERE YOU WANT IT.
		vec3 skyPositionOffset = vec3(0.0, 0.0, -500.0);

		// 3. Apply the offset to the vertex position
		//    We apply 'model' first to handle rotation/scale of the mesh itself,
		//    then add our sky offset.
		vec4 world_pos = model * vec4(displacedPos*500, 1.0);
		world_pos.xyz += skyPositionOffset;

		// 4. Project using the static view
		gl_Position = projection * staticView * world_pos;

		// 5. Force to Far Plane (Optional, keeps it behind everything)
		gl_Position = gl_Position.xyww;

		// --- CRITICAL FIX FOR CLIPPING ---
		// Your clip plane logic relies on 'FragPos'. If FragPos is at 0,0,0
		// (because we didn't update it), the clip plane might hide the object.
		// We must update FragPos to match the new "Sky" position.
		// FragPos = vec3(model * vec4(displacedPos*200, 1.0));
		FragPos = world_pos.xyz;
	} else {
		// --- Standard Object Logic ---
		gl_Position = projection * view * vec4(FragPos, 1.0);
	}

	gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
}
