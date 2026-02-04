#version 430 core

in vec3 vs_fragPos;
in vec3 vs_p1;
in vec3 vs_p2;
in float vs_r1;
in float vs_r2;
in vec3 vs_segmentColor;

out vec4 FragColor;

#include "helpers/lighting.glsl"

uniform mat4 view;
uniform mat4 projection;

float sdCapsule(vec3 p, vec3 a, vec3 b, float r1, float r2) {
    vec3 pa = p - a, ba = b - a;
    float d_ba = dot(ba, ba);
    float h = (d_ba < 1e-6) ? 0.0 : clamp(dot(pa, ba) / d_ba, 0.0, 1.0);
    float r = mix(r1, r2, h);
    return length(pa - ba * h) - r;
}

vec3 getNormal(vec3 p) {
    float d = sdCapsule(p, vs_p1, vs_p2, vs_r1, vs_r2);
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        sdCapsule(p + e.xyy, vs_p1, vs_p2, vs_r1, vs_r2) - d,
        sdCapsule(p + e.yxy, vs_p1, vs_p2, vs_r1, vs_r2) - d,
        sdCapsule(p + e.yyx, vs_p1, vs_p2, vs_r1, vs_r2) - d
    ));
}

void main() {
    vec3 rayOrigin = viewPos;
    vec3 rayDir = normalize(vs_fragPos - viewPos);

    // Bounds for raymarching
    vec3 center = (vs_p1 + vs_p2) * 0.5;
    float distToCenter = length(center - rayOrigin);
    float segmentLen = length(vs_p1 - vs_p2);
    float maxR = max(vs_r1, vs_r2);
    float boundRadius = segmentLen * 0.5 + maxR + 0.1;

    float t = max(0.0, distToCenter - boundRadius);
    float maxT = length(vs_fragPos - rayOrigin); // vs_fragPos is on the back face

    bool hit = false;
    vec3 p;
    for(int i = 0; i < 40; ++i) {
        p = rayOrigin + rayDir * t;
        float d = sdCapsule(p, vs_p1, vs_p2, vs_r1, vs_r2);
        if (d < 0.001) {
            hit = true;
            break;
        }
        t += d;
        if (t > maxT) break;
    }

    if (!hit) discard;

    vec3 normal = getNormal(p);

    // Lighting
    vec4 litColor = apply_lighting_no_shadows(p, normal, vs_segmentColor, 0.5);

    // Depth correction to ensure proper intersection with other geometry
    vec4 clipPos = projection * view * vec4(p, 1.0);
    gl_FragDepth = (clipPos.z / clipPos.w) * 0.5 + 0.5;

    // Soft edges based on thickness
    float alpha = 0.9;
    FragColor = vec4(litColor.rgb, alpha);
}
