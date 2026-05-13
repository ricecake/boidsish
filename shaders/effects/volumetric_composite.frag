#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D uSceneTexture;
layout(binding = 1) uniform sampler2D uDepthTexture;
layout(binding = 2) uniform sampler3D uVolumetricTexture;

#include "../types/temporal_data.glsl"

const int NUM_CASCADES = 4;
const int GRID_RES_Z = 64;
const float CASCADE_DISTANCES[4] = { 20.0, 60.0, 200.0, 1000.0 };

void main() {
    float depth = texture(uDepthTexture, TexCoords).r;
    vec3 sceneColor = texture(uSceneTexture, TexCoords).rgb;

    // Reconstruct view-space depth
    float z_ndc = depth * 2.0 - 1.0;
    vec4 clipPos = vec4(TexCoords * 2.0 - 1.0, z_ndc, 1.0);
    vec4 viewPos = invProjection * clipPos;
    viewPos /= viewPos.w;

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
        float w = (float(cascade) * float(GRID_RES_Z) + (slice * 63.5 + 0.5)) / float(GRID_RES_Z * NUM_CASCADES);

        vec4 vol = texture(uVolumetricTexture, vec3(TexCoords, w));
        scattering = vol.rgb;
        transmittance = vol.a;
    } else {
        // Beyond last cascade
        float w = (float(NUM_CASCADES * GRID_RES_Z) - 0.5) / float(GRID_RES_Z * NUM_CASCADES);
        vec4 vol = texture(uVolumetricTexture, vec3(TexCoords, w));
        scattering = vol.rgb;
        transmittance = vol.a;
    }

    // Apply volumetric lighting
    vec3 result = sceneColor * transmittance + scattering;

    FragColor = vec4(result, 1.0);
}
