#version 430 core

layout(location = 0) out float FragShadow;

in mat4 v_invModel;

uniform sampler2D u_depthTexture;
uniform sampler2D u_hizTexture;
uniform sampler3D u_sdfTexture;
uniform vec3      u_sdfExtent;
uniform vec3      u_sdfMin;

uniform vec3  u_worldLightDir;
uniform float u_sdfShadowSoftness = 10.0;
uniform float u_sdfShadowMaxDist = 2.0;
uniform float u_sdfShadowBias = 0.05;

#include "temporal_data.glsl"

vec3 worldPosFromDepth(float depth, vec2 uv) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = invView * invProjection * clipPos;
    return worldPos.xyz / worldPos.w;
}

void main() {
    vec2 uv = gl_FragCoord.xy * texelSize;
    float depth = texture(u_depthTexture, uv).r;

    // Check if this pixel is sky
    if (depth >= 1.0) {
        discard;
    }

    vec3 worldPos = worldPosFromDepth(depth, uv);

    // Transform world position to local space
    vec3 localPos = (v_invModel * vec4(worldPos, 1.0)).xyz;

    // Transform light direction to local space
    vec3 localLightDir = normalize(mat3(v_invModel) * u_worldLightDir);

    float shadow = 1.0;
    float t = u_sdfShadowBias;

    // Simple raymarch
    for (int i = 0; i < 32; ++i) {
        vec3 p = localPos + localLightDir * t;

        // Map local space to SDF UV
        vec3 sdfUv = (p - u_sdfMin) / u_sdfExtent;

        if (any(lessThan(sdfUv, vec3(0.0))) || any(greaterThan(sdfUv, vec3(1.0)))) {
            break; // Ray left the volume
        }

        float dist = texture(u_sdfTexture, sdfUv).r * length(u_sdfExtent);

        if (dist < 0.01) {
            shadow = 0.0;
            break;
        }

        shadow = min(shadow, u_sdfShadowSoftness * dist / t);

        t += max(0.01, dist);
        if (t > u_sdfShadowMaxDist) break;
    }

    FragShadow = clamp(shadow, 0.0, 1.0);
}
