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
    vec4      minBound;
    vec4      maxBound;
	int       numSources;
	SdfSource sources[128];
};

#include "lygia/generative/fbm.glsl"

// Fast ray-AABB intersection
bool rayAABB(vec3 ro, vec3 rd, vec3 minB, vec3 maxB, out float tMin, out float tMax) {
    vec3 invRd = 1.0 / (rd + 1e-6);
    vec3 t0 = (minB - ro) * invRd;
    vec3 t1 = (maxB - ro) * invRd;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    tMin = max(max(tmin.x, tmin.y), tmin.z);
    tMax = min(min(tmax.x, tmax.y), tmax.z);
    return tMax > max(tMin, 0.0);
}

float getDensity(vec3 p, out vec3 color) {
    float d = 1000.0;
    color = vec3(0.0);

    if (numSources == 0) return 0.0;

    // Union of all SDF sources
    for (int i = 0; i < numSources; ++i) {
        vec3 diff = p - sources[i].position_radius.xyz;
        float dist = length(diff) - sources[i].position_radius.w;

        // Smooth union for density blending
        float k = sources[i].color_smoothness.a;
        float h = clamp(0.5 + 0.5 * (d - dist) / k, 0.0, 1.0);
        d = mix(d, dist, h) - k * h * (1.0 - h);
        color = mix(color, sources[i].color_smoothness.rgb, h);
    }

    if (d > 2.0) return 0.0; // Early out if far from density

    // Apply FBM noise for cloudiness
    float noise = fbm(p * 0.3 + time * 0.15);
    float detail = fbm(p * 0.8 - time * 0.1) * 0.4;

    return smoothstep(1.5, -1.0, d) * max(0.0, noise + detail);
}

// Simple volumetric shadow transmittance
float getShadow(vec3 p, vec3 lightDir) {
    float shadow = 1.0;
    float t = 0.5;
    vec3 dummyColor;
    for(int i = 0; i < 4; i++) {
        vec3 sp = p + lightDir * t;
        float d = getDensity(sp, dummyColor);
        shadow *= exp(-d * 0.5);
        t += 1.5;
        if (shadow < 0.1) break;
    }
    return shadow;
}

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

    float tMin, tMax;
    if (!rayAABB(viewPos, rayDir, minBound.xyz, maxBound.xyz, tMin, tMax)) {
        FragColor = vec4(sceneColor, 1.0);
        return;
    }

    tMin = max(tMin, 0.1);
    tMax = min(tMax, sceneDistance);

    if (tMax <= tMin) {
        FragColor = vec4(sceneColor, 1.0);
        return;
    }

    // Volumetric Integration
    const int SAMPLES = 16;
    float stepSize = (tMax - tMin) / float(SAMPLES);

    // Dithering to hide banding
    float offset = fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453);
    float t = tMin + offset * stepSize;

    vec3 col = vec3(0.0);
    float transmittance = 1.0;

    vec3 sunDir = normalize(lights[0].direction);
    vec3 sunColor = lights[0].color * lights[0].intensity;

    // Phase function parameter (forward scattering for clouds)
    const float G = 0.5;
    float cosTheta = dot(rayDir, -sunDir);
    float phaseValue = (1.0 - G*G) / (4.0 * PI * pow(1.0 + G*G - 2.0 * G * cosTheta, 1.5));

    for (int i = 0; i < SAMPLES; i++) {
        vec3 p = viewPos + rayDir * t;
        vec3 sampleCol;
        float density = getDensity(p, sampleCol);

        if (density > 0.01) {
            float shadow = getShadow(p, -sunDir);
            vec3 light = ambient_light + sunColor * shadow * phaseValue;

            vec3 scattering = sampleCol * density * light;

            // Energy conserving integration
            float ext = density * 0.8;
            vec3 sampleInscattering = scattering * (1.0 - exp(-ext * stepSize)) / max(ext, 0.001);
            col += transmittance * sampleInscattering;
            transmittance *= exp(-ext * stepSize);
        }

        t += stepSize;
        if (transmittance < 0.01) break;
    }

    vec3 finalColor = sceneColor * transmittance + col;
    FragColor = vec4(finalColor, 1.0);
}
