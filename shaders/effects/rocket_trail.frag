#version 420 core

out vec4 FragColor;
in vec2  TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler3D voxelTexture;

uniform vec3  cameraPos;
uniform mat4  invView;
uniform mat4  invProjection;
uniform float time;

uniform vec3  gridOrigin;
uniform vec3  gridSize;
uniform float voxelSize;
uniform float maxAge;

#include "../helpers/noise.glsl"

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;

	float z = depth * 2.0 - 1.0;
	vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	vec3 worldPos = (invView * viewSpacePosition).xyz;

	vec3  rayDir = normalize(worldPos - cameraPos);
	float dist = length(worldPos - cameraPos);

	if (depth == 1.0) {
		dist = 1000.0;
		worldPos = cameraPos + rayDir * dist;
	}

	// AABB intersection with the voxel grid
	vec3 boxMin = gridOrigin;
	vec3 boxMax = gridOrigin + gridSize;

	vec3 invDir = 1.0 / (rayDir + 1e-9);
	vec3 t0 = (boxMin - cameraPos) * invDir;
	vec3 t1 = (boxMax - cameraPos) * invDir;
	vec3 tmin_v = min(t0, t1);
	vec3 tmax_v = max(t0, t1);

	float t_start = max(max(tmin_v.x, tmin_v.y), tmin_v.z);
	float t_end = min(min(tmax_v.x, tmax_v.y), tmax_v.z);

	t_start = max(t_start, 0.0);
	t_end = min(t_end, dist);

	vec3  trailColor = vec3(0.0);
	float trailAlpha = 0.0;

	if (t_start < t_end) {
		int   samples = 64;
		float stepSize = (t_end - t_start) / float(samples);
		float t = t_start + fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453) * stepSize;

		for (int i = 0; i < samples; i++) {
			vec3 p = cameraPos + rayDir * t;
			vec3 uvw = (p - gridOrigin) / gridSize;

			float birthTime = texture(voxelTexture, uvw).r;
			if (birthTime > 0.0) {
				float age = time - birthTime;
				if (age >= 0.0 && age < maxAge) {
					float lifeFactor = 1.0 - age / maxAge;

					// Expansion: density decreases and noise scale might change
					float density = lifeFactor * 1.5;

					// Detail noise
					float n = snoise(p * 0.4 - time * 0.1);
					density *= smoothstep(-0.4, 0.6, n);

					float sampleAlpha = density * stepSize * 0.8;
					vec3  sampleColor = vec3(0.85, 0.85, 0.9); // Slight bluish smoke

					trailColor += (1.0 - trailAlpha) * sampleAlpha * sampleColor;
					trailAlpha += (1.0 - trailAlpha) * sampleAlpha;
				}
			}
			t += stepSize;
			if (trailAlpha > 0.98)
				break;
		}
	}

	FragColor = vec4(sceneColor * (1.0 - trailAlpha) + trailColor, 1.0);
}
