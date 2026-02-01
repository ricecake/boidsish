#version 420 core

out vec4 FragColor;
in vec2  TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform vec2      screenSize;
uniform mat4      invView;
uniform mat4      invProjection;

// The project's Lighting UBO and helpers
#include "helpers/lighting.glsl"

// SdfVolumes UBO
struct SdfSource {
	vec4 position_radius;  // xyz: pos, w: radius
	vec4 color_smoothness; // rgb: color, a: smoothness
	vec4 params;           // x: charge, y: type, zw: unused
};

layout(std140) uniform SdfVolumes {
	int       numSources;
	SdfSource sources[128];
};

// Lygia configuration
#define RESOLUTION screenSize
#define CAMERA_POSITION viewPos

#include "lygia/sdf/sphereSDF.glsl"
#include "lygia/generative/fbm.glsl"
#include "lygia/lighting/medium/new.glsl"

// We define our own volumetric map function for lygia
Medium volumetricMap(vec3 p) {
    Medium res = mediumNew();
    float d = 1000.0;
    vec3 col = vec3(0.0);

    // Union of all SDF sources
    for (int i = 0; i < numSources; ++i) {
        float dist = sphereSDF(p - sources[i].position_radius.xyz, sources[i].position_radius.w);

        // Smooth union for density blending
        float k = sources[i].color_smoothness.a;
        float h = clamp(0.5 + 0.5 * (d - dist) / k, 0.0, 1.0);
        d = mix(d, dist, h) - k * h * (1.0 - h);
        col = mix(col, sources[i].color_smoothness.rgb, h);
    }

    // Apply FBM noise for cloudiness
    // We use a combination of frequencies for a more cloud-like appearance
    float noise = fbm(p * 0.4 + time * 0.1);
    float detailNoise = fbm(p * 1.5 - time * 0.05) * 0.5;

    // Density is higher where d is negative, modulated by noise
    float density = smoothstep(2.0, -2.0, d) * max(0.0, noise + detailNoise);

    res.sdf = -density;
    res.scattering = col;
    res.absorption = vec3(0.05);

    return res;
}

#define RAYMARCH_VOLUME_MAP_FNC volumetricMap
#define RAYMARCH_VOLUME
#define RAYMARCH_VOLUME_SAMPLES 64
#define RAYMARCH_VOLUME_SAMPLES_LIGHT 8
#define RAYMARCH_MIN_DIST 0.1
#define RAYMARCH_MAX_DIST 500.0
#define RAYMARCH_ENERGY_CONSERVING

// We'll define a custom shadow transmittance that uses our lighting system
// For now, use the first directional light as the main sun for clouds
#define LIGHT_DIRECTION lights[0].direction
#define LIGHT_COLOR     lights[0].color
#define LIGHT_INTENSITY lights[0].intensity

#include "lygia/lighting/raymarch/volume.glsl"

void main() {
    vec3  sceneColor = texture(sceneTexture, TexCoords).rgb;
	float depth = texture(depthTexture, TexCoords).r;

	// Reconstruct scene world position
	vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 vPos = invProjection * ndcPos;
	vPos /= vPos.w;
	vec4  worldPos = invView * vPos;
	float sceneDistance = length(worldPos.xyz - viewPos);
	if (depth >= 0.999999)
		sceneDistance = 1000.0;

	// Ray direction
	vec4 target = invProjection * vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec3 rayDir = normalize((invView * vec4(normalize(target.xyz), 0.0)).xyz);

    // Call lygia's raymarchVolume
    vec3 color = raymarchVolume(viewPos, rayDir, TexCoords, sceneDistance, sceneColor);

    FragColor = vec4(color, 1.0);
}
