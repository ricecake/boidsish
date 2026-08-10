#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec3 uCameraPos;
uniform vec3 uCameraFront;
uniform float uViewScale;
uniform vec2 uViewOffset;

uniform int uSelectedLayer; // 0: Combined, 1: Cloud SDF, 2: LBM, 3: Shadow
uniform bool uShowWind;
uniform bool uShowTemperature;
uniform bool uShowHumidity;
uniform bool uShowPressure;
uniform bool uShowAerosol;
uniform bool uShowCloudCover;
uniform bool uShowCloudHeight;
uniform bool uShowCloudShadow;

uniform sampler2D uCloudWeatherTex;
uniform sampler2D uLbmWindTex;
uniform sampler2D uLbmScalarTex;
uniform sampler2D uLbmAerosolTex;
uniform sampler2D uCloudShadowTex;

uniform ivec2 uLbmGridOrigin;
uniform ivec2 uLbmGridSize;
uniform float uLbmSpacing;
uniform float uCloudShadowWorldSize;
uniform float uWorldScale;

void main() {
    // Determine world position from TexCoords and view settings
    // Default view range 200km at scale 1.0
    float range = 100000.0 / uViewScale;
    vec2 worldXZ = uViewOffset + (TexCoords - 0.5) * range;

    vec3 combinedColor = vec4(0.1, 0.1, 0.1, 1.0).rgb;

    // --- 1. Cloud Weather Layer ---
    // CloudWeather Bake covers 100,000 range around origin (0,0) normally
    // Actually from cloud_weather_bake.comp: p = vec3(uv.x * mapRange, 0.0, uv.y * mapRange);
    // where mapRange = 100000.0 * uWorldScale;
    // It's [0, mapRange] and repeats.
    float mapRange = 100000.0 * uWorldScale;
    vec2 cloudUV = worldXZ / mapRange;
    vec4 cloudData = texture(uCloudWeatherTex, cloudUV);
    float cloudSdf = cloudData.r;
    float cloudHeight = cloudData.g;
    float cloudThickness = cloudData.a;
    float coverage = smoothstep(500.0, -500.0, cloudSdf);

    if (uSelectedLayer == 1) {
        combinedColor = vec3(coverage);
        if (uShowCloudHeight) combinedColor.g = cloudHeight;
        // Heatmap for SDF
        if (cloudSdf > 0.0) combinedColor.rb += vec2(cloudSdf / 5000.0, 0.0);
        else combinedColor.gb += vec2(0.0, abs(cloudSdf) / 5000.0);
    } else if (uSelectedLayer == 0 && uShowCloudCover) {
        combinedColor = mix(combinedColor, vec3(1.0, 1.0, 1.0), coverage * 0.5);
    }

    // --- 2. LBM Weather Layer ---
    vec2 lbmLocalPos = (worldXZ - vec2(uLbmGridOrigin.x, uLbmGridOrigin.y) * uLbmSpacing);
    vec2 lbmUV = lbmLocalPos / (vec2(uLbmGridSize) * uLbmSpacing);

    if (lbmUV.x >= 0.0 && lbmUV.x <= 1.0 && lbmUV.y >= 0.0 && lbmUV.y <= 1.0) {
        vec4 wind = texture(uLbmWindTex, lbmUV);
        vec4 scalars = texture(uLbmScalarTex, lbmUV);
        vec4 aerosols = texture(uLbmAerosolTex, lbmUV);

        if (uSelectedLayer == 2) {
            combinedColor = vec3(0.0);
            if (uShowTemperature) combinedColor.r = (scalars.x - 273.15) / 50.0;
            if (uShowHumidity) combinedColor.g = scalars.y;
            if (uShowPressure) combinedColor.b = (scalars.z - 950.0) / 100.0;
            if (uShowWind) combinedColor += vec3(length(wind.xz) / 20.0);
            if (uShowAerosol) combinedColor += aerosols.rgb;
        } else if (uSelectedLayer == 0) {
            if (uShowTemperature) combinedColor = mix(combinedColor, vec3(1.0, 0.2, 0.0), (scalars.x - 273.15) / 100.0);
            if (uShowWind) {
                float v = length(wind.xz) / 30.0;
                combinedColor += vec3(0.0, v * 0.5, v);
            }
        }
    } else if (uSelectedLayer == 2) {
        combinedColor = vec3(0.1, 0.0, 0.0); // Outside simulation bounds
    }

    // --- 3. Cloud Shadow Layer ---
    float shadowMapSize = uCloudShadowWorldSize;
    vec2 shadowUV = (worldXZ - (uCameraPos.xz - shadowMapSize * 0.5)) / shadowMapSize;
    if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
        float shadow = texture(uCloudShadowTex, shadowUV).r;
        if (uSelectedLayer == 3) {
            combinedColor = vec3(1.0 - shadow);
        } else if (uSelectedLayer == 0 && uShowCloudShadow) {
            combinedColor *= mix(1.0, 0.5, shadow);
        }
    }

    // --- 4. Camera Indicator ---
    float distToCam = length(worldXZ - uCameraPos.xz);
    float indicatorSize = range * 0.015;
    if (distToCam < indicatorSize) {
        combinedColor = vec3(1.0, 1.0, 0.0);

        // Orientation line
        vec2 dir = normalize(uCameraFront.xz);
        vec2 toPoint = (worldXZ - uCameraPos.xz);
        float alongDir = dot(toPoint, dir);
        float perpDir = length(toPoint - dir * alongDir);
        if (alongDir > 0.0 && alongDir < indicatorSize * 3.0 && perpDir < indicatorSize * 0.2) {
            combinedColor = vec3(1.0, 1.0, 1.0);
        }
    }

    // Grid lines
    vec2 grid = abs(fract(worldXZ / 5000.0 + 0.5) - 0.5) / fwidth(worldXZ / 5000.0);
    float line = min(grid.x, grid.y);
    combinedColor = mix(combinedColor, vec3(0.3), 1.0 - smoothstep(0.0, 1.0, line));

    FragColor = vec4(combinedColor, 1.0);
}
