#ifndef HELPERS_VOLUMETRIC_LIGHTING_GLSL
#define HELPERS_VOLUMETRIC_LIGHTING_GLSL

layout(std140, binding = [[VOLUMETRIC_LIGHTING_BINDING]]) uniform VolumetricLightingParams {
    mat4 u_volViewProjection;
    mat4 u_volInvViewProjection;
    mat4 u_volView;
    mat4 u_volProjection;
    vec4 u_volCameraPosNear; // xyz, w: near
    vec4 u_volCameraDirFar;  // xyz, w: far
    ivec4 u_volGridResPad;   // xyz, w: pad
};

uniform sampler3D u_volumetricLightTexture;

vec3 sampleVolumetricLight(vec3 worldPos) {
    vec4 clipPos = u_volViewProjection * vec4(worldPos, 1.0);
    vec3 ndcPos = clipPos.xyz / clipPos.w;
    vec2 uv = ndcPos.xy * 0.5 + 0.5;

    // Logarithmic Z mapping
    float viewZ = dot(worldPos - u_volCameraPosNear.xyz, u_volCameraDirFar.xyz);
    if (viewZ < u_volCameraPosNear.w || viewZ > u_volCameraDirFar.w) return vec3(0.0);

    float slice = log2(viewZ / u_volCameraPosNear.w) / log2(u_volCameraDirFar.w / u_volCameraPosNear.w);

    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || slice < 0.0 || slice > 1.0) {
        return vec3(0.0);
    }

    return texture(u_volumetricLightTexture, vec3(uv, slice)).rgb * 10.0;
}

#endif
