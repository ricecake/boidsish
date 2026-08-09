#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform mat4 uInvView;

layout(binding = 0) uniform sampler2D uSceneTexture;
layout(binding = 1) uniform sampler2D uDepthTexture;
layout(binding = [[VOLUMETRIC_SCATTERING_BINDING]]) uniform sampler3D uVolumetricTexture;

uniform bool uUse2DAccumulation;
uniform sampler2D uVolumetricHistory2D;

#include "../types/temporal_data.glsl"
#include "../helpers/lighting.glsl"

#include "lygia/color/palette.glsl"
// #include "lygia/generative/voronoi.glsl"
// #include "lygia/generative/snoise.glsl"

const int NUM_CASCADES = 4;
const int GRID_RES_Z = 64;
const float CASCADE_DISTANCES[4] = { 20.0, 60.0, 200.0, 1000.0 };

void main() {
    float depth = texture(uDepthTexture, TexCoords).r;
    vec3 sceneColor = texture(uSceneTexture, TexCoords).rgb;

    // Reconstruct view-space depth
    float z_ndc = depth * 2.0 - 1.0;
    vec4 clipPos = vec4(TexCoords * 2.0 - 1.0, z_ndc, 1.0);
    vec4 vsPos = invProjection * clipPos;
    vsPos /= (abs(vsPos.w) > 0.0001) ? vsPos.w : 1.0;
    vec3 worldPos = (uInvView * vsPos).xyz;

    float linearZ = max(0.1, -vsPos.z);
    if (depth >= 1.0) linearZ = 1000.0; // Sample far end for sky

    vec3 cameraPos = viewPos; // Global camera position from types/lighting.glsl
    vec3 rayDir = normalize(worldPos - cameraPos);
    float dist = length(worldPos - cameraPos);

    if (rayDir.y < 0.0 && cameraPos.y > 0.0) {
        float t_floor = cameraPos.y / max(-rayDir.y, 0.00001);
        if (t_floor < dist) {
            float scale = t_floor / max(0.0001, dist);
            linearZ = min(linearZ, max(0.1, -vsPos.z * scale));
        }
    }

    // Find cascade
    int cascade = -1;
    float z_near = 0.1;
    float z_far = 0.0;

    for (int i = 0; i < NUM_CASCADES; ++i) {
        if (linearZ <= CASCADE_DISTANCES[i]) {
            cascade = i;
            z_far = CASCADE_DISTANCES[i];
            if (i > 0) z_near = CASCADE_DISTANCES[i-1];
            break;
        }
    }

    vec3 scattering = vec3(0.0);
    float transmittance = 1.0;

    if (uUse2DAccumulation) {
        vec4 vol = texture(uVolumetricHistory2D, TexCoords);
        scattering = vol.rgb;
        transmittance = vol.a;
    } else {
        if (cascade != -1) {
            // Calculate W coordinate for this cascade
            float slice = clamp(log(linearZ / z_near) / log(z_far / z_near), 0.0, 1.0);

            // Continuous mapping across all cascades to ensure smooth linear filtering
            float w = (float(cascade) + slice) / float(NUM_CASCADES);

            vec4 vol = texture(uVolumetricTexture, vec3(TexCoords, w));
            scattering = vol.rgb;
            transmittance = vol.a;
        } else {
            // Beyond last cascade - sample the very edge
            vec4 vol = texture(uVolumetricTexture, vec3(TexCoords, 1.0));
            scattering = max(vec3(0.0), vol.rgb);
            transmittance = clamp(vol.a, 0.0, 1.0);
        }
    }

    if (any(isnan(scattering))) scattering = vec3(0.0);
    if (isnan(transmittance)) transmittance = 1.0;

    // Apply volumetric lighting
    vec3 result = sceneColor * transmittance + scattering;

    // Refined Scene Mask: height-dependent sky/not-sky classification
    int isSky = 0;
    if (depth > 0.99999) {
        if (rayDir.y < 0.0 && cameraPos.y > 0.0) {
            float t = -cameraPos.y / rayDir.y;
            float maxSceneDist = mix(10000.0 * worldScale, 700.0 * worldScale, smoothstep(0.0, 1500.0 * worldScale, cameraPos.y));
            if (t < maxSceneDist) {
                isSky = 0;
            } else {
                isSky = 1;
            }
        } else {
            isSky = 1;
        }
    } else {
        isSky = 0;
    }

    float sceneMask = 1.0 - float(isSky);

    FragColor = vec4(result, sceneMask);
}
