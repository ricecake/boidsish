#ifndef VOLUMETRIC_LIGHTING_GLSL
#define VOLUMETRIC_LIGHTING_GLSL

layout(std140, binding = [[VOLUMETRIC_LIGHTING_BINDING]]) uniform VolumetricLighting {
    mat4 view;
    mat4 projection;
    mat4 invViewProj;
    vec4 gridSize;      // x, y, z, num_cascades
    vec4 clipParams;    // x=near, y=far, z=log_base, w=worldScale
    vec4 cascadeFars;   // x, y, z, w (matches std140 array layout)
    vec4 hazeParams;    // x=density, y=height, z=mie_anisotropy, w=unused
    vec4 hazeColor;     // rgb=color, w=unused
    vec4 cloudParams;   // x=altitude, y=thickness, z=density, w=coverage
    vec4 cloudParams2;  // x=warp, y=time, z=unused, w=unused
} volUbo;

uniform sampler3D u_volumetricIntegrated0;
uniform sampler3D u_volumetricIntegrated1;
uniform sampler3D u_volumetricIntegrated2;

/**
 * Samples the integrated volumetric lighting for a given screen UV and planar depth.
 * depth: Planar distance from camera (dot(worldPos - cameraPos, cameraFront))
 * Returns: vec4(accumulatedScattering.rgb, accumulatedTransmittance.a)
 */
vec4 getVolumetricLighting(vec2 uv, float depth) {
    vec3 totalScattering = vec3(0.0);
    float totalTransmittance = 1.0;

    float cascadeFars[3];
    cascadeFars[0] = volUbo.cascadeFars.x;
    cascadeFars[1] = volUbo.cascadeFars.y;
    cascadeFars[2] = volUbo.cascadeFars.z;

    for (int i = 0; i < 3; ++i) {
        float near = (i == 0) ? volUbo.clipParams.x : cascadeFars[i - 1];
        float far = cascadeFars[i];

        if (depth > near) {
            float sampleDepth = min(depth, far);
            float zCoord = clamp(log(sampleDepth / near) / log(far / near), 0.0, 1.0);

            vec4 data;
            if (i == 0) data = texture(u_volumetricIntegrated0, vec3(uv, zCoord));
            else if (i == 1) data = texture(u_volumetricIntegrated1, vec3(uv, zCoord));
            else data = texture(u_volumetricIntegrated2, vec3(uv, zCoord));

            totalScattering += data.rgb * totalTransmittance;
            totalTransmittance *= data.a;
        }

        if (depth <= far) break;
    }

    return vec4(totalScattering, totalTransmittance);
}

#endif // VOLUMETRIC_LIGHTING_GLSL
