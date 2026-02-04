#version 430 core

in vec3 vs_fragPos;
in vec3 vs_p1;
in vec3 vs_p2;
in float vs_r1;
in float vs_r2;
in vec3 vs_segmentColor;

out vec4 FragColor;

#include "helpers/lighting.glsl"

float sdCapsule(vec3 p, vec3 a, vec3 b, float r1, float r2) {
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float r = mix(r1, r2, h);
    return length(pa - ba * h) - r;
}

void main() {
    float d = sdCapsule(vs_fragPos, vs_p1, vs_p2, vs_r1, vs_r2);

    // Wider threshold for smooth anti-aliasing
    float edgeWidth = fwidth(d);
    if (d > edgeWidth) discard;

    // Calculate normal from gradient of SDF
    vec3 ba = vs_p2 - vs_p1;
    vec3 pa = vs_fragPos - vs_p1;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    vec3 closestPoint = vs_p1 + ba * h;
    vec3 normal = normalize(vs_fragPos - closestPoint);

    // Lighting
    vec4 litColor = apply_lighting_no_shadows(vs_fragPos, normal, vs_segmentColor, 0.5);

    // Anti-aliasing at edges
    float alpha = smoothstep(edgeWidth, -edgeWidth, d);

    FragColor = vec4(litColor.rgb, alpha * 0.8);
}
