#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthTexture;
uniform sampler3D cloudBaseNoise;
uniform sampler3D cloudDetailNoise;
uniform sampler2D weatherMap;
uniform sampler3D curlNoise;
uniform sampler2D historyTexture;

uniform float u_cloudHeight;
uniform float u_cloudThickness;
uniform float u_cloudDensity;
uniform float u_cloudCoverage;
uniform float u_cloudWarp;
uniform float u_cloudType;
uniform float u_warpPush;

#include "../lighting.glsl"
#include "../helpers/noise.glsl"
#include "../atmosphere/common.glsl"
#include "../temporal_data.glsl"

#define CLOUD_START_HEIGHT u_cloudHeight
#define CLOUD_END_HEIGHT (u_cloudHeight + u_cloudThickness)

// Simple Ray-Plane intersection for cloud layer
bool rayPlaneIntersection(vec3 ro, vec3 rd, float h, out float t) {
    if (abs(rd.y) < 1e-6) return false;
    t = (h - ro.y) / rd.y;
    return t >= 0.0;
}

float get_density(vec3 p, vec3 weather) {
    float height_fraction = (p.y - CLOUD_START_HEIGHT) / max(u_cloudThickness, 1.0);
    if (height_fraction < 0.0 || height_fraction > 1.0) return 0.0;

    // Warp p to "push" clouds away
    vec3 push_vec = normalize(p - viewPos);
    p += push_vec * u_warpPush * 2000.0;

    vec4 base_noise = texture(cloudBaseNoise, p * 0.00005 + time * 0.005);
    float low_freq_fbm = base_noise.g * 0.625 + base_noise.b * 0.25 + base_noise.a * 0.125;
    float base_cloud = remap(base_noise.r, -(1.0 - low_freq_fbm), 1.0, 0.0, 1.0);

    float coverage = weather.r * u_cloudCoverage;
    float density = remap(base_cloud, clamp(1.0 - coverage, 0.01, 0.99), 1.0, 0.0, 1.0);
    density *= coverage;

    // Shaping based on height
    float vertical_shape = smoothstep(0.0, 0.1, height_fraction) * smoothstep(1.0, 0.9, height_fraction);
    density *= vertical_shape;

    if (density > 0.01) {
        vec3 curl = texture(curlNoise, p * 0.0005).rgb;
        p += curl * height_fraction * u_cloudWarp * 500.0;

        vec4 detail_noise = texture(cloudDetailNoise, p * 0.0005 + time * 0.02);
        float high_freq_fbm = detail_noise.r * 0.625 + detail_noise.g * 0.25 + detail_noise.b * 0.125;

        float modifier = mix(high_freq_fbm, 1.0 - high_freq_fbm, clamp(height_fraction * 10.0, 0.0, 1.0));
        density = remap(density, modifier * 0.2, 1.0, 0.0, 1.0);
    }

    return clamp(density * u_cloudDensity, 0.0, 1.0);
}

float light_energy(vec3 p, vec3 L, vec3 weather) {
    float shadow_steps = 4.0;
    float shadow_step_size = u_cloudThickness / shadow_steps;
    float total_density = 0.0;
    for (int i = 0; i < int(shadow_steps); i++) {
        vec3 sp = p + L * (float(i) + 0.5) * shadow_step_size;
        total_density += get_density(sp, weather);
    }
    return exp(-total_density * shadow_step_size * 0.005);
}

float blue_noise_jitter(vec2 p) {
    p = floor(p);
    p += 1337.0 * fract(float(frameIndex) * 0.1);
    p = floor(p/5.5)*5.5;
    p = fract(p * 0.1) + 1.0 + p * vec2(0.0002, 0.0003);
    float a = fract(1.0 / (0.000001 * p.x * p.y + 0.00001));
    a = fract(1.0 / (0.000001234 * a + 0.00001));
    return a;
}

void main() {
    float depth = texture(depthTexture, TexCoords).r;

    float z = depth * 2.0 - 1.0;
    vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
    vec4  viewSpacePosition = invProjection * clipSpacePosition;
    viewSpacePosition /= viewSpacePosition.w;
    vec3 worldPos = (invView * viewSpacePosition).xyz;

    vec3  rayDir = normalize(worldPos - viewPos);
    float sceneDist = length(worldPos - viewPos);
    if (depth == 1.0) sceneDist = 100000.0;

    float t_start, t_end;
    bool hit_start = rayPlaneIntersection(viewPos, rayDir, CLOUD_START_HEIGHT, t_start);
    bool hit_end = rayPlaneIntersection(viewPos, rayDir, CLOUD_END_HEIGHT, t_end);

    if (!hit_start && !hit_end) {
        FragColor = vec4(0.0);
        return;
    }

    float t_min = min(t_start, t_end);
    float t_max = max(t_start, t_end);
    t_min = max(t_min, 0.0);
    t_max = min(t_max, min(sceneDist, 50000.0));

    if (t_min >= t_max) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 sunDir = normalize(vec3(0, 1, 0));
    vec3 sunColor = vec3(1);
    if (num_lights > 0) {
        sunDir = normalize(-lights[0].direction);
        sunColor = lights[0].color * lights[0].intensity;
    }

    float jitter = blue_noise_jitter(gl_FragCoord.xy);
    int   steps = 48; // Balanced for performance and quality
    float step_size = (t_max - t_min) / float(steps);
    float t = t_min + jitter * step_size;

    float transmittance = 1.0;
    vec3  lighting = vec3(0.0);
    float cos_theta = dot(rayDir, sunDir);
    // Henyey-Greenstein
    float g = 0.6;
    float phase = (1.0 - g*g) / (4.0 * PI * pow(1.0 + g*g - 2.0 * g * cos_theta, 1.5));

    for (int i = 0; i < steps; i++) {
        if (transmittance <= 0.01) break;

        vec3 p = viewPos + rayDir * t;
        vec3 weather = texture(weatherMap, p.xz * 0.00001).rgb;
        float d = get_density(p, weather);

        if (d > 0.01) {
            float e = light_energy(p, sunDir, weather);
            vec3  step_light = sunColor * e * phase + ambient_light * 0.15;
            float extinction = d * 0.05;
            float sample_transmittance = exp(-extinction * step_size);

            lighting += transmittance * (1.0 - sample_transmittance) * step_light;
            transmittance *= sample_transmittance;
        }

        t += step_size;
    }

    lighting = clamp(lighting, 0.0, 10.0);
    float opacity = clamp(1.0 - transmittance, 0.0, 1.0);

    // Robustness: check for NaN/Inf
    if (any(isnan(lighting)) || isnan(opacity) || any(isinf(lighting)) || isinf(opacity)) {
        lighting = vec3(0.0);
        opacity = 0.0;
    }

    vec4 currentCloud = vec4(lighting, opacity);

    // Temporal Reprojection
    vec3 cloudPlanePos = viewPos + rayDir * mix(t_min, t_max, 0.5);
    vec4 prevClipPos = prevViewProjection * vec4(cloudPlanePos, 1.0);
    vec2 prevUV = (prevClipPos.xy / prevClipPos.w) * 0.5 + 0.5;

    if (prevUV.x >= 0.0 && prevUV.x <= 1.0 && prevUV.y >= 0.0 && prevUV.y <= 1.0) {
        vec4 history = texture(historyTexture, prevUV);
        if (!any(isnan(history)) && !any(isinf(history))) {
            float alpha = (frameIndex < 30) ? 0.3 : 0.95;
            // Only mix if history is not zero to avoid black patches spreading
            if (history.a > 0.0) {
                currentCloud = mix(currentCloud, history, alpha);
            }
        }
    }

    FragColor = currentCloud;
}
