#version 450 core

layout(location = 0) out vec4 FragColor;

in vec3 v_worldPos;
in vec3 v_viewDir;

layout(binding = 0) uniform sampler3D u_mass;
layout(binding = 1) uniform sampler3D u_velocity;
layout(binding = 2) uniform sampler2D u_depthTexture;

uniform vec3 u_cameraPos;
uniform ivec3 u_resolution;
uniform vec3 u_worldScale;
uniform vec3 u_worldOrigin;
uniform vec3 u_waterColor;

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

    vec3 boxMin = u_worldOrigin;
    vec3 boxMax = u_worldOrigin + u_worldScale;

    float tnear, tfar;
    if (intersect_aabb(ro, rd, boxMin, boxMax, tnear, tfar) > 1e20) {
        discard;
    }

    tnear = max(tnear, 0.0);

    // Raymarching
    const int MAX_STEPS = 128;
    float stepSize = (tfar - tnear) / float(MAX_STEPS);
    float t = tnear;

    vec4 sum = vec4(0.0);

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = ro + rd * t;
        vec3 uvw = (p - u_worldOrigin) / u_worldScale;

        float mass = texture(u_mass, uvw).r;

        if (mass > 0.1) {
            // Simplified shading
            float alpha = mass * 0.5;
            vec4 col = vec4(u_waterColor, alpha);
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
