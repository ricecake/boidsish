#version 460 core

out vec4 FragColor;
in vec2 TexCoords;

#include "../helpers/constants.glsl"

#define USE_TERRAIN_DATA
#include "../helpers/terrain_shadows.glsl"
#include "../helpers/lighting.glsl"

uniform mat4 invProjection;
uniform mat4 invView;

uniform vec3 u_probePosition;
uniform float u_probeRadius;
uniform vec3 u_probeColor;
uniform float u_probeMetallic;
uniform float u_probeRoughness;
uniform bool u_useSHVisualizer;

uniform sampler2D depthTexture;

struct SpaceProbeData {
	float shadow_level;
	float shadow_from_terrain;
	float cloud_shadow;
	float ambient_occlusion;
	vec4 light_directional;
	vec4 light_other;
	vec4 light_ambient;
	vec4 sh_coeffs[9];
};

layout(std430, binding = [[SPACE_PROBE_BINDING]]) buffer SpaceProbeBuffer {
	SpaceProbeData u_spaceProbeData;
};

// SH basis constants
const float C1 = 0.282095;
const float C2 = 0.488603;
const float C3 = 1.092548;
const float C4 = 0.315392;
const float C5 = 0.546274;

vec3 evalProbeSH(vec3 n) {
	float x = n.x;
	float y = n.y;
	float z = n.z;

	vec3 color = u_spaceProbeData.sh_coeffs[0].rgb * C1;
	color += u_spaceProbeData.sh_coeffs[1].rgb * (C2 * y);
	color += u_spaceProbeData.sh_coeffs[2].rgb * (C2 * z);
	color += u_spaceProbeData.sh_coeffs[3].rgb * (C2 * x);
	color += u_spaceProbeData.sh_coeffs[4].rgb * (C3 * x * y);
	color += u_spaceProbeData.sh_coeffs[5].rgb * (C3 * y * z);
	color += u_spaceProbeData.sh_coeffs[6].rgb * (C4 * (3.0 * y * y - 1.0));
	color += u_spaceProbeData.sh_coeffs[7].rgb * (C3 * x * z);
	color += u_spaceProbeData.sh_coeffs[8].rgb * (C5 * (x * x - z * z));

	return max(color, vec3(0.0));
}

void main() {
	// Reconstruct scene depth and world position
	float depth = texture(depthTexture, TexCoords).r;

	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 viewPosFromDepth = invProjection * ndcPos;
	viewPosFromDepth /= viewPosFromDepth.w;
	vec4  worldPos = invView * viewPosFromDepth;
	float sceneDistance = length(worldPos.xyz - viewPos);
	if (depth >= 0.999999) {
		sceneDistance = 1000000.0;
	}

	// Camera ray direction
	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

	// Sphere raycast
	vec3 oc = viewPos - u_probePosition;
	float b = dot(oc, rayDir);
	float c = dot(oc, oc) - u_probeRadius * u_probeRadius;
	float h = b * b - c;
	if (h < 0.0) {
		discard;
	}

	float t = -b - sqrt(h);
	if (t < 0.0) {
		t = -b + sqrt(h);
	}
	if (t < 0.0 || t > sceneDistance) {
		discard;
	}

	// We have a valid hit on the sphere overlay
	vec3 hitPos = viewPos + rayDir * t;
	vec3 N = normalize(hitPos - u_probePosition);

	// Calculate correct depth and write to gl_FragDepth
	vec4 clipPos = projection * view * vec4(hitPos, 1.0);
	float ndcDepth = clipPos.z / clipPos.w;
	gl_FragDepth = ndcDepth * 0.5 + 0.5;

	// Evaluate SH at the normal
	vec3 shColor = evalProbeSH(N);

	vec3 finalColor;
	if (u_useSHVisualizer) {
		finalColor = shColor;
	} else {
		// Beautiful PBR-inspired shading using the SH probe and scene lights
		vec3 albedo = u_probeColor;
		float roughness = u_probeRoughness;
		float metallic = u_probeMetallic;

		vec3 directLight = vec3(0.0);
		vec3 V = normalize(viewPos - hitPos);

		for (int i = 0; i < num_lights; ++i) {
			vec3 L;
			float atten;
			calculateLightContribution(i, hitPos, L, atten);

			float NdotL = max(dot(N, L), 0.0);
			if (NdotL > 0.0) {
				float shadow = 1.0;
				if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
					shadow = calculateShadow(i, hitPos, N, L);
					shadow *= calculateCloudShadowFactor(hitPos, L, cloudShadowIntensity);
				} else if ((lights[i].flags & LIGHT_FLAG_CASTS_SHADOW) != 0) {
					shadow = calculateShadow(i, hitPos, N, L);
				}

				// Simple diffuse and microfacet specular reflection approximation
				vec3 H = normalize(V + L);
				float NdotH = max(dot(N, H), 0.0);
				float spec = pow(NdotH, mix(8.0, 128.0, 1.0 - roughness));
				vec3 specularColor = mix(vec3(0.04), albedo, metallic);

				directLight += lights[i].color * lights[i].intensity * atten * shadow * (albedo * NdotL + specularColor * spec);
			}
		}

		vec3 ambientTerm = albedo * shColor * (1.0 - metallic);
		finalColor = ambientTerm + directLight;
	}

	// Tone mapping and gamma correction if needed (or standard shader color)
	FragColor = vec4(finalColor, 1.0);
}
