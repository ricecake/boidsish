#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D uSceneTexture;
layout(binding = 1) uniform sampler2D uDepthTexture;
layout(binding = [[VOLUMETRIC_SCATTERING_BINDING]]) uniform sampler3D uVolumetricTexture;

#include "../types/temporal_data.glsl"
#include "../helpers/volumetric_common.glsl"

void main() {
    ivec3 grid_res = ivec3(160, 90, 64); // Matches VolumetricLightingEffect.h
    float depth = texture(uDepthTexture, TexCoords).r;
    vec3 sceneColor = texture(uSceneTexture, TexCoords).rgb;

    // Reconstruct view-space depth
    float z_ndc = depth * 2.0 - 1.0;
    vec4 clipPos = vec4(TexCoords * 2.0 - 1.0, z_ndc, 1.0);
    vec4 viewPos = invProjection * clipPos;
    viewPos /= (abs(viewPos.w) > 0.0001) ? viewPos.w : 1.0;

    float linearZ = max(0.1, -viewPos.z);
    if (depth >= 1.0) linearZ = 1000.0; // Sample far end for sky

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

    if (cascade != -1) {
        // Calculate W coordinate for this cascade
        float slice = clamp(log(linearZ / z_near) / log(z_far / z_near), 0.0, 1.0);
        float w = (float(cascade * grid_res.z) + (slice * float(grid_res.z - 1) + 0.5)) / float(grid_res.z * NUM_CASCADES);

        vec4 vol = texture(uVolumetricTexture, vec3(TexCoords, w));
        scattering = vol.rgb;
        transmittance = vol.a;
    } else {
        // Beyond last cascade
        float w = (float(NUM_CASCADES * grid_res.z) - 0.5) / float(grid_res.z * NUM_CASCADES);
        vec4 vol = texture(uVolumetricTexture, vec3(TexCoords, w));
        scattering = max(vec3(0.0), vol.rgb);
        transmittance = clamp(vol.a, 0.0, 1.0);
    }

    if (any(isnan(scattering))) scattering = vec3(0.0);
    if (isnan(transmittance)) transmittance = 1.0;

    // Apply volumetric lighting only to the sky (depth >= 1.0)
    // Surface lighting now handles its own volumetric integration for grounding.
    vec3 result = sceneColor;
    // if (depth >= 0.99999) {
        result = sceneColor * transmittance + scattering;
    // }

    // Preserve Scene Mask in alpha channel: 1.0 for scene, 0.0 for sky
    float sceneMask = (depth < 0.99999) ? 1.0 : 0.0;

    FragColor = vec4(result, sceneMask);
}
