#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
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

layout(std140, binding = 6) uniform TemporalData_Clouds {
	mat4  currentViewProjection;
	mat4  prevViewProjection;
	mat4  uProjection;
	mat4  invProjection_Clouds;
	mat4  invView_Clouds;
	vec2  texelSize;
	int   frameIndex;
	float padding_clouds;
};

#define EARTH_RADIUS_METERS 6360000.0
#define CLOUD_START_HEIGHT u_cloudHeight
#define CLOUD_END_HEIGHT (u_cloudHeight + u_cloudThickness)

float remap_local(float value, float low1, float high1, float low2, float high2) {
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

// Ray-Sphere intersection for cloud layer
bool raySphereIntersection(vec3 ro, vec3 rd, float r, out float t) {
    vec3  center = vec3(0, -EARTH_RADIUS_METERS, 0);
    vec3  oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - r * r;
    float h = b * b - c;
    if (h < 0.0) return false;
    h = sqrt(h);
    t = -b + h;
    if (t < 0.0) t = -b + h;
    return t >= 0.0;
}

float get_density(vec3 p, vec3 weather) {
    float height_fraction = (p.y - CLOUD_START_HEIGHT) / u_cloudThickness;
    if (height_fraction < 0.0 || height_fraction > 1.0) return 0.0;

    // Warp p to "push" clouds away
    vec3 push_vec = normalize(p - viewPos);
    p += push_vec * u_warpPush * 1000.0;

    vec4 base_noise = texture(cloudBaseNoise, p * 0.0001 + time * 0.01);
    float low_freq_fbm = base_noise.g * 0.625 + base_noise.b * 0.25 + base_noise.a * 0.125;
    float base_cloud = remap_local(base_noise.r, -(1.0 - low_freq_fbm), 1.0, 0.0, 1.0);

    float coverage = weather.r * u_cloudCoverage;
    float density = remap_local(base_cloud, 1.0 - coverage, 1.0, 0.0, 1.0);
    density *= coverage;

    // Shaping based on height
    float vertical_shape = smoothstep(0.0, 0.1, height_fraction) * smoothstep(1.0, 0.9, height_fraction);
    density *= vertical_shape;

    if (density > 0.0) {
        vec3 curl = texture(curlNoise, p * 0.001).rgb;
        p += curl * height_fraction * u_cloudWarp * 500.0;

        vec4 detail_noise = texture(cloudDetailNoise, p * 0.001 + time * 0.05);
        float high_freq_fbm = detail_noise.r * 0.625 + detail_noise.g * 0.25 + detail_noise.b * 0.125;

        float modifier = mix(high_freq_fbm, 1.0 - high_freq_fbm, clamp(height_fraction * 10.0, 0.0, 1.0));
        density = remap_local(density, modifier * 0.2, 1.0, 0.0, 1.0);
    }

    return clamp(density * u_cloudDensity, 0.0, 1.0);
}

float beer_powder(float d) {
    return exp(-d) * (1.0 - exp(-d * 2.0));
}

float henyey_greenstein(float cos_theta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cos_theta, 1.5));
}

float light_energy(vec3 p, vec3 L, vec3 weather) {
    float shadow_steps = 6.0;
    float shadow_step_size = u_cloudThickness / shadow_steps;
    float total_density = 0.0;
    for (int i = 0; i < int(shadow_steps); i++) {
        vec3 sp = p + L * (float(i) + 0.5) * shadow_step_size;
        total_density += get_density(sp, weather);
    }
    return beer_powder(total_density * shadow_step_size * 0.01);
}

void main() {
    float depth = texture(depthTexture, TexCoords).r;

    float z = depth * 2.0 - 1.0;
    vec4  clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
    vec4  viewSpacePosition = invProjection_Clouds * clipSpacePosition;
    viewSpacePosition /= viewSpacePosition.w;
    vec3 worldPos = (invView_Clouds * viewSpacePosition).xyz;

    vec3  rayDir = normalize(worldPos - viewPos);
    float sceneDist = length(worldPos - viewPos);
    if (depth == 1.0) sceneDist = 1e10;

    float t_start, t_end;
    bool hit_start = raySphereIntersection(viewPos, rayDir, EARTH_RADIUS_METERS + CLOUD_START_HEIGHT, t_start);
    bool hit_end = raySphereIntersection(viewPos, rayDir, EARTH_RADIUS_METERS + CLOUD_END_HEIGHT, t_end);

    if (!hit_start && !hit_end) {
        FragColor = vec4(0.0);
        return;
    }

    float t_min = min(t_start, t_end);
    float t_max = max(t_start, t_end);
    t_min = max(t_min, 0.0);
    t_max = min(t_max, sceneDist);

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

    float jitter = fract(sin(dot(TexCoords, vec2(12.9898, 78.233))) * 43758.5453);
    int   steps = 64;
    float step_size = (t_max - t_min) / float(steps);
    float t = t_min + jitter * step_size;

    float opacity = 0.0;
    vec3  lighting = vec3(0.0);
    float cos_theta = dot(rayDir, sunDir);
    float phase = mix(henyey_greenstein(cos_theta, 0.8), henyey_greenstein(cos_theta, -0.2), 0.5);

    for (int i = 0; i < steps; i++) {
        if (opacity >= 0.99) break;

        vec3 p = viewPos + rayDir * t;
        vec3 weather = texture(weatherMap, p.xz * 0.00001).rgb;
        float d = get_density(p, weather);

        if (d > 0.0) {
            float e = light_energy(p, sunDir, weather);
            vec3  step_light = sunColor * e * phase + ambient_light * 0.1;
            float sample_opacity = d * step_size * 0.01;

            lighting += (1.0 - opacity) * sample_opacity * step_light;
            opacity += (1.0 - opacity) * sample_opacity;
        }

        t += step_size;
    }

    vec4 currentCloud = vec4(lighting, opacity);

    // Temporal Reprojection
    vec4 prevClipPos = prevViewProjection * vec4(worldPos, 1.0);
    vec2 prevUV = (prevClipPos.xy / prevClipPos.w) * 0.5 + 0.5;

    if (prevUV.x >= 0.0 && prevUV.x <= 1.0 && prevUV.y >= 0.0 && prevUV.y <= 1.0) {
        vec4 history = texture(historyTexture, prevUV);
        currentCloud = mix(history, currentCloud, 0.05);
    }

    FragColor = currentCloud;
}
