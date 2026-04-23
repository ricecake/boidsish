#version 450 core

layout(location = 0) out vec4 FragColor;

in vec3 v_worldPos;

layout(binding = 0) uniform sampler3D u_mass;
layout(binding = 1) uniform sampler3D u_velocity;
layout(binding = 2) uniform sampler2D u_depthTexture;

uniform vec3 u_cameraPos;
uniform ivec3 u_resolution;
uniform vec3 u_worldScale;
uniform vec3 u_worldOrigin;
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

float get_scene_depth(vec2 uv) {
    float z = texture(u_depthTexture, uv).r;
    // Standard conversion from non-linear depth to linear world distance
    // This is a placeholder; actual conversion depends on near/far planes
    // For now, we'll use a simplified depth test if linear depth isn't easily available
    return z;
}

void main() {
    vec2 screenUV = gl_FragCoord.xy / textureSize(u_depthTexture, 0);
    float sceneDepth = texture(u_depthTexture, screenUV).r;

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

        // Scene depth test
        vec4 projPos = projection * view * vec4(p, 1.0);
        float pDepth = (projPos.z / projPos.w) * 0.5 + 0.5;
        if (pDepth > sceneDepth) break;

        vec3 uvw = (p - u_worldOrigin) / u_worldScale;
        float mass = texture(u_mass, uvw).r;

        if (mass > 0.01) {
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
