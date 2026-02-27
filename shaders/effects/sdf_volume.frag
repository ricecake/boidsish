#version 430 core

out vec4 FragColor;
in vec2  TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform vec2      screenSize;
uniform vec3      cameraPos;
uniform mat4      invView;
uniform mat4      invProjection;
uniform float     time;

uniform vec3      sdfMin;
uniform vec3      sdfMax;
uniform int       numPositiveSources;
uniform int       numNegativeSources;

struct SdfSource {
	vec4 position_radius;  // xyz: pos, w: radius
	vec4 color_smoothness; // rgb: color, a: smoothness
	vec4 params;           // x: charge, y: type, zw: unused
};

layout(std430, binding = 5) buffer SdfVolumes {
	SdfSource sources[];
};

#include "lygia/sdf/sphereSDF.glsl"

// Custom union that handles color blending
vec4 opUnionColored(vec4 d1, vec4 d2, float k) {
	float h = clamp(0.5 + 0.5 * (d2.a - d1.a) / k, 0.0, 1.0);
	float res_d = mix(d2.a, d1.a, h) - k * h * (1.0 - h);
	vec3  res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

// Custom subtraction that handles color for "antimatter" effect
vec4 opSubtractionColored(vec4 d1, vec4 d2, float k) {
	float h = clamp(0.5 - 0.5 * (d2.a + d1.a) / k, 0.0, 1.0);
	float res_d = mix(d2.a, -d1.a, h) + k * h * (1.0 - h);
	// Blend predator color into the blob where it's being "eaten"
	vec3 res_col = mix(d2.rgb, d1.rgb, h);
	return vec4(res_col, res_d);
}

vec4 map(vec3 p) {
	vec4 res = vec4(1.0, 1.0, 1.0, 1000.0);

	// First pass: Union of positive charges (Boids)
	if (numPositiveSources > 0) {
		float d = sphereSDF(p - sources[0].position_radius.xyz, sources[0].position_radius.w);
		res = vec4(sources[0].color_smoothness.rgb, d);

		for (int i = 1; i < numPositiveSources; ++i) {
			float di = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);
			res = opUnionColored(vec4(sources[i].color_smoothness.rgb, di), res, sources[i].color_smoothness.a);
		}
	}

	// Second pass: Subtraction of negative charges (Predators)
	for (int i = 0; i < numNegativeSources; ++i) {
		int idx = numPositiveSources + i;
		float d = sphereSDF(p - sources[idx].position_radius.xyz, sources[idx].position_radius.w);
		res = opSubtractionColored(
			vec4(sources[idx].color_smoothness.rgb, d),
			res,
			sources[idx].color_smoothness.a
		);
	}

	return res;
}

vec3 getNormal(vec3 p) {
	vec2 e = vec2(0.01, 0.0);
	vec3 n = vec3(
		map(p + e.xyy).a - map(p - e.xyy).a,
		map(p + e.yxy).a - map(p - e.yxy).a,
		map(p + e.yyx).a - map(p - e.yyx).a
	);
	float len = length(n);
	return len > 0.0001 ? n / len : vec3(0, 1, 0);
}

float sdBox(vec3 p, vec3 b) {
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

void main() {
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;

	if (numPositiveSources == 0 && numNegativeSources == 0) {
		FragColor = vec4(sceneColor, 1.0);
		return;
	}

	float depth = texture(depthTexture, TexCoords).r;

	// Reconstruct scene world position to get depth limit
	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPos = invProjection * ndcPos;
	viewPos /= viewPos.w;
	vec4  worldPos = invView * viewPos;
	float sceneDistance = length(worldPos.xyz - cameraPos);
	if (depth >= 0.999999)
		sceneDistance = 10000.0;

	// Ray direction
	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

	// Optimization: Global AABB check
	vec3 boxCenter = (sdfMin + sdfMax) * 0.5;
	vec3 boxHalfExtents = (sdfMax - sdfMin) * 0.5 + vec3(15.0); // Add margin for smoothness

	float t = 0.0;

	float distToBox = sdBox(cameraPos - boxCenter, boxHalfExtents);
	if (distToBox > 0.0) {
		t = distToBox;
	}

	vec4  res;
	bool  hit = false;

	// Stabilize loop
	for (int i = 0; i < 96; ++i) {
		if (t > sceneDistance || t > 1500.0)
			break;

		vec3 p = cameraPos + rayDir * t;

		res = map(p);
		if (res.a < 0.005) { // Tightened hit threshold
			hit = true;
			break;
		}

		// Ensure positive progress
		t += max(res.a, 0.001);
	}

	if (hit) {
		vec3  p = cameraPos + rayDir * t;
		vec3  normal = getNormal(p);
		vec3  lightDir = normalize(vec3(0.5, 1.0, 0.5));
		float diff = max(dot(normal, lightDir), 0.0);

		float rim = 1.0 - max(dot(normal, -rayDir), 0.0);
		rim = pow(rim, 3.0);

		vec3 volumeColor = res.rgb * (diff * 0.8 + 0.2) + res.rgb * rim * 0.5;

		FragColor = vec4(volumeColor, 1.0);
	} else {
		FragColor = vec4(sceneColor, 1.0);
	}
}
