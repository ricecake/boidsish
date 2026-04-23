#version 450 core

layout(location = 0) out vec4 FragColor;

in vec3 v_worldPos;

#include "helpers/lbm_common.glsl"

layout(binding = 0) uniform sampler3D u_mass;
layout(binding = 1) uniform sampler3D u_velocity;
layout(binding = 2) uniform sampler2D u_depthTexture;

uniform vec3 u_cameraPos;
uniform vec3 u_waterColor;
uniform mat4 view;
uniform mat4 projection;

float intersect_aabb(vec3 ro, vec3 rd, vec3 boxMin, vec3 boxMax, out float tnear, out float tfar) {
    vec3 tMin = (boxMin - ro) / rd;
    vec3 tMax = (boxMax - ro) / rd;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    tnear = max(max(t1.x, t1.y), t1.z);
    tfar = min(min(t2.x, t2.y), t2.z);
    return (tnear <= tfar && tfar > 0.0) ? tnear : 1e30;
}

void main() {
    vec3 ro = u_cameraPos;
    vec3 rd = normalize(v_worldPos - u_cameraPos);

    vec3 boxMin = u_worldOrigin.xyz;
    vec3 boxMax = u_worldOrigin.xyz + u_worldScale.xyz;

    float tnear, tfar;
    if (intersect_aabb(ro, rd, boxMin, boxMax, tnear, tfar) > 1e20) {
        discard;
    }

    tnear = max(tnear, 0.0);

    ivec2 screenCoord = ivec2(gl_FragCoord.xy);
    float sceneDepth = texelFetch(u_depthTexture, screenCoord, 0).r;

    const int MAX_STEPS = 64;
    float stepSize = (tfar - tnear) / float(MAX_STEPS);
    float t = tnear;

    vec4 sum = vec4(0.0);

    // Optimization: Calculate matrix products once
    mat4 vp = projection * view;

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = ro + rd * t;

        vec4 projPos = vp * vec4(p, 1.0);
        float pDepth = (projPos.z / projPos.w) * 0.5 + 0.5;
        if (pDepth > sceneDepth) break;

        vec3 uvw = (p - u_worldOrigin.xyz) / u_worldScale.xyz;
        float mass = texture(u_mass, uvw).r;

        if (mass > 0.02) {
            float normalizedMass = clamp(mass / u_lbmParams.z, 0.0, 1.0);
            float alpha = smoothstep(0.02, 0.1, normalizedMass) * 0.25;

            // Fresnel
            float fresnel = 1.0 - max(0.0, dot(-rd, vec3(0,1,0)));
            alpha += fresnel * 0.1;

            float diff = 0.7 + 0.3 * max(0.0, dot(normalize(vec3(0.5, 1.0, 0.2)), vec3(0,1,0)));

            vec4 col = vec4(u_waterColor * diff, alpha);
            col.rgb *= col.a;
            sum += col * (1.0 - sum.a);
        }

        if (sum.a > 0.95) break;
        t += stepSize;
        if (t > tfar) break;
    }

    if (sum.a < 0.01) discard;

    FragColor = sum;
}
