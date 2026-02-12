#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D normalTexture;
uniform sampler2D pbrTexture;
uniform sampler2D hizTexture;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 invProjection;
uniform mat4 invView;
uniform float time;
uniform uint  frameCount;
uniform vec3 viewPos;

// Parameters
uniform int   maxSteps = 64;
uniform float maxDistance = 500.0;
uniform float jitterStrength = 0.2;
uniform float thicknessBias = 0.1;

#include "helpers/blue_noise.glsl"

// Hammersley sequence for low-discrepancy 2D points
float radicalInverse_VdC(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 hammersley(uint i, uint N) {
	return vec2(float(i) / float(N), radicalInverse_VdC(i));
}

// GGX Importance Sampling
vec3 importanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
	float a = roughness * roughness;

	float phi = 2.0 * 3.14159265359 * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// From spherical coordinates to cartesian coordinates
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// From tangent-space vector to world-space sample vector
	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

vec3 getPos(vec2 uv) {
	float depth = texture(depthTexture, uv).r;
	vec4  clipSpacePosition = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4  viewSpacePosition = invProjection * clipSpacePosition;
	return viewSpacePosition.xyz / viewSpacePosition.w;
}

vec3 getNormal(vec2 uv) {
	return texture(normalTexture, uv).xyz;
}

// Hi-Z Raymarching
bool hiZTrace(vec3 startVS, vec3 dirVS, out vec2 hitUV) {
    // Project ray to screen space
    vec4 startClip = projection * vec4(startVS, 1.0);
    startClip.xyz /= startClip.w;
    vec3 startSS = vec3(startClip.xy * 0.5 + 0.5, startClip.z);

    // End point in view space
    vec3 endVS = startVS + dirVS * maxDistance;
    // Clip end point to near plane if necessary
    if (endVS.z > -0.1) endVS.z = -0.1;

    vec4 endClip = projection * vec4(endVS, 1.0);
    endClip.xyz /= endClip.w;
    vec3 endSS = vec3(endClip.xy * 0.5 + 0.5, endClip.z);

    vec3 dirSS = endSS - startSS;

    // Clamp to screen boundaries
    float tMax = 1.0;
    if (dirSS.x > 0.0) tMax = min(tMax, (1.0 - startSS.x) / dirSS.x);
    else if (dirSS.x < 0.0) tMax = min(tMax, (0.0 - startSS.x) / dirSS.x);
    if (dirSS.y > 0.0) tMax = min(tMax, (1.0 - startSS.y) / dirSS.y);
    else if (dirSS.y < 0.0) tMax = min(tMax, (0.0 - startSS.y) / dirSS.y);
    if (dirSS.z > 0.0) tMax = min(tMax, (1.0 - startSS.z) / dirSS.z);
    else if (dirSS.z < 0.0) tMax = min(tMax, (0.0 - startSS.z) / dirSS.z);

    dirSS *= tMax;

    ivec2 screenRes = textureSize(hizTexture, 0);
    int maxLevel = int(floor(log2(float(max(screenRes.x, screenRes.y)))));
    int level = 0;
    float t = 0.001;

    for (int i = 0; i < maxSteps; i++) {
        vec3 currentSS = startSS + dirSS * t;
        if (currentSS.x < 0.0 || currentSS.x > 1.0 || currentSS.y < 0.0 || currentSS.y > 1.0 || currentSS.z > 1.0 || t > 1.0) break;

        float minZ = textureLod(hizTexture, currentSS.xy, level).r;

        if (currentSS.z < minZ) {
            // Skip cell
            ivec2 cellCount = screenRes >> level;
            vec2 cellIdx = floor(currentSS.xy * vec2(cellCount));
            vec2 cellStart = cellIdx / vec2(cellCount);
            vec2 cellEnd = (cellIdx + 1.0) / vec2(cellCount);

            // Find t to exit current cell
            vec2 tExit2 = vec2(1e10);
            if (abs(dirSS.x) > 1e-6) tExit2.x = ((dirSS.x > 0.0 ? cellEnd.x : cellStart.x) - startSS.x) / dirSS.x;
            if (abs(dirSS.y) > 1e-6) tExit2.y = ((dirSS.y > 0.0 ? cellEnd.y : cellStart.y) - startSS.y) / dirSS.y;

            float tExit = min(tExit2.x, tExit2.y);
            t = tExit + 0.0001;
            level = min(maxLevel, level + 1);
        } else {
            if (level == 0) {
                float actualDepth = textureLod(depthTexture, currentSS.xy, 0).r;
                // Hit check with thickness
                if (currentSS.z >= actualDepth && (currentSS.z - actualDepth) < (thicknessBias / 1000.0)) {
                    hitUV = currentSS.xy;
                    return true;
                }
                t += 0.001;
            } else {
                level--;
            }
        }
    }
    return false;
}

void main() {
	float depth = texture(depthTexture, TexCoords).r;
	if (depth >= 1.0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

	vec3 posVS = getPos(TexCoords);
	vec3 worldNormal = normalize(getNormal(TexCoords));
	vec2 pbr = texture(pbrTexture, TexCoords).rg;
	float roughness = pbr.r;

	vec3 viewDirVS = normalize(posVS);
	vec3 normalVS = normalize((view * vec4(worldNormal, 0.0)).xyz);

	// Stochastic part: use Hammersley Sequence + GGX Importance Sampling
	// Use frameCount to rotate through the sequence
	uint  sampleIndex = frameCount % 64u;
	vec2  Xi = hammersley(sampleIndex, 64u);

	// Use blue noise to further jitter the Xi to avoid patterns
	float bn = blueNoise(TexCoords * textureSize(sceneTexture, 0), time);
	Xi = fract(Xi + vec2(bn, blueNoise(TexCoords * 1.1, time + 0.5)));

	// Importance sample GGX to get a microfacet normal (half-vector)
	vec3 H_VS = importanceSampleGGX(Xi, normalVS, roughness * jitterStrength);
	vec3 reflectDirVS = reflect(viewDirVS, H_VS);

	// Skip if reflecting back into surface
	if (dot(normalVS, reflectDirVS) < 0.0) {
		FragColor = texture(sceneTexture, TexCoords);
		return;
	}

    vec2 hitUV;
    bool hit = hiZTrace(posVS, reflectDirVS, hitUV);

	vec4 sceneColor = texture(sceneTexture, TexCoords);
    vec4 hitColor = hit ? texture(sceneTexture, hitUV) : vec4(0.0);

	// Fresnel
	float fresnel = pow(1.0 - max(dot(normalVS, -viewDirVS), 0.0), 5.0);

	// Reflection strength
	float reflectionStrength = mix(0.5, 0.1, roughness);
	reflectionStrength = mix(reflectionStrength, 1.0, fresnel);

	// Edge fade
	vec2  dUV = abs(TexCoords - 0.5) * 2.0;
	float screenFade = 1.0 - clamp(max(pow(dUV.x, 8.0), pow(dUV.y, 8.0)), 0.0, 1.0);

    // Ray hit UV edge fade
    float rayFade = 0.0;
    if (hit) {
	    vec2  hitDUV = abs(hitUV - 0.5) * 2.0;
        rayFade = 1.0 - clamp(max(pow(hitDUV.x, 8.0), pow(hitDUV.y, 8.0)), 0.0, 1.0);
    }

	float finalReflectionFactor = reflectionStrength * (hit ? 1.0 : 0.0) * screenFade * rayFade;

    if (hit && distance(hitUV, TexCoords) < 0.005) finalReflectionFactor = 0.0;

	FragColor = mix(sceneColor, hitColor, finalReflectionFactor);
}
