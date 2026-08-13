#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec3 uCameraPos;
uniform vec3 uCameraFront;
uniform float uViewScale;
uniform vec2 uViewOffset;

uniform int uSelectedLayer; // 0: Combined, 1: Raw Cloud Map, 2: Cloud Effective Coverage, 3: Deep Opacity Map, 4: LBM Simulation, 5: Terrain Height, 6: Terrain Color
uniform bool uShowWind;
uniform bool uShowTemperature;
uniform bool uShowHumidity;
uniform bool uShowPressure;
uniform bool uShowAerosol;
uniform bool uShowCloudCover;
uniform bool uShowCloudHeight;
uniform bool uShowCloudShadow;

uniform sampler2D uCloudWeatherTex;
uniform sampler2D uBakedWindTex;
uniform sampler2D uLbmWindTex;
uniform sampler2D uLbmScalarTex;
uniform sampler2D uLbmAerosolTex;
uniform sampler2DArray uCloudShadowTex; // 2D Array

uniform ivec2 uLbmGridOrigin;
uniform ivec2 uLbmGridSize;
uniform float uLbmSpacing;
uniform float uWorldScale;
uniform float uTime;
uniform mat4 uCloudShadowMatrix;

#include "helpers/constants.glsl"
#include "helpers/terrain_common.glsl"

void main() {
    // Determine world position from TexCoords and view settings
    float range = 100000.0 / uViewScale;
    vec2 worldXZ = uViewOffset + (TexCoords - 0.5) * range;

    // Default background is very dark gray/black
    vec3 combinedColor = vec3(0.05, 0.05, 0.05);

    // --- 1. Terrain Layer ---
    TerrainSurface surface;
    surface.height = -10000.0;
    surface.normal = vec3(0.0, 1.0, 0.0);
    bool hasTerrain = (u_originSize.w >= 1);

    if (hasTerrain) {
        surface = getTerrainSurface(worldXZ);
    }

    vec3 terrainBaseColor = vec3(0.12, 0.12, 0.12);
    vec3 terrainShaded = vec3(0.12, 0.12, 0.12);

    if (hasTerrain && surface.height > -9000.0) {
        // Calculate procedural terrain coloring
        float h = surface.height;
        float slope = surface.normal.y;

        vec3 terrainCol = vec3(0.15, 0.12, 0.1);
        if (h < 0.0) {
            // Water / Shore
            terrainCol = mix(vec3(0.08, 0.35, 0.55), vec3(0.7, 0.65, 0.5), smoothstep(-15.0, 0.0, h));
        } else {
            // Land
            vec3 sand = vec3(0.78, 0.72, 0.52);
            vec3 grass = vec3(0.22, 0.45, 0.12);
            vec3 rock = vec3(0.42, 0.4, 0.38);
            vec3 snow = vec3(0.92, 0.92, 0.95);

            if (h < 15.0) {
                terrainCol = mix(sand, grass, smoothstep(2.0, 15.0, h));
            } else if (h < 500.0) {
                terrainCol = mix(grass, rock, smoothstep(200.0, 500.0, h));
            } else {
                terrainCol = mix(rock, snow, smoothstep(600.0, 1000.0, h));
            }

            // Steep cliff rock blend
            terrainCol = mix(rock, terrainCol, smoothstep(0.4, 0.7, slope));
        }

        terrainBaseColor = terrainCol;

        // Apply a nice top-left directional light shading
        vec3 lightDir = normalize(vec3(-1.0, 2.0, -1.0));
        float diff = max(0.25, dot(surface.normal, lightDir));
        terrainShaded = terrainBaseColor * diff;
    }

    // --- 2. Cloud and Shadow Data ---
    float mapRange = 100000.0 * uWorldScale;
    vec2 cloudUV = worldXZ / mapRange;
    vec4 cloudData = texture(uCloudWeatherTex, cloudUV);
    // cloudData: R=dist, G=height, B=thickness, A=density

    // Project for Cloud Shadow Map (sampler2DArray)
    vec4 lightSpacePos = uCloudShadowMatrix * vec4(worldXZ.x, 0.0, worldXZ.y, 1.0);
    vec2 shadowUV = lightSpacePos.xy * 0.5 + 0.5;

    float shadowDensity = 0.0;
    float shadowTransmittance = 1.0;

    if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
        shadowDensity = textureLod(uCloudShadowTex, vec3(shadowUV, 7.0), 1.5).r;
        shadowTransmittance = exp(-shadowDensity * 0.001 * 2.0 / max(0.001, uWorldScale));
    }

    // --- 3. Render Selected Layer ---
    if (uSelectedLayer == 0) { // Combined View
        // Start with Terrain base
        vec3 groundColor = terrainShaded;

        // Fetch LBM / Wind data
        vec2 lbmLocalPos = (worldXZ - vec2(uLbmGridOrigin.x, uLbmGridOrigin.y) * uLbmSpacing);
        vec2 lbmUV = lbmLocalPos / (vec2(uLbmGridSize) * uLbmSpacing);
        bool inLbmBounds = (lbmUV.x >= 0.0 && lbmUV.x <= 1.0 && lbmUV.y >= 0.0 && lbmUV.y <= 1.0);
        vec4 wind = vec4(0.0);

        if (inLbmBounds) {
            wind = texture(uBakedWindTex, lbmUV);
            vec4 scalars = texture(uLbmScalarTex, lbmUV);
            vec4 aerosols = texture(uLbmAerosolTex, lbmUV);

            // Overlay ground/weather diagnostics (Temperature, Humidity, Aerosol) onto terrain
            if (uShowTemperature) {
                float tempC = scalars.x - 273.15;
                vec3 tempColor = mix(vec3(0.0, 0.2, 0.8), vec3(0.8, 0.1, 0.0), clamp((tempC + 10.0) / 50.0, 0.0, 1.0));
                groundColor = mix(groundColor, tempColor, 0.35);
            }
            if (uShowHumidity) {
                vec3 humidColor = mix(vec3(0.8, 0.8, 0.5), vec3(0.1, 0.4, 0.7), scalars.y);
                groundColor = mix(groundColor, humidColor, 0.35);
            }
            if (uShowAerosol) {
                vec3 dustCol   = vec3(0.7, 0.4, 0.15) * clamp(aerosols.r * 15.0, 0.0, 1.0);
                vec3 pollenCol = vec3(0.5, 0.8, 0.1)  * clamp(aerosols.g * 15.0, 0.0, 1.0);
                vec3 smokeCol  = vec3(0.3, 0.3, 0.35) * clamp(aerosols.b * 15.0, 0.0, 1.0);
                vec3 mistCol   = vec3(0.4, 0.7, 0.95) * clamp(aerosols.a * 15.0, 0.0, 1.0);
                groundColor = mix(groundColor, groundColor + dustCol + pollenCol + smokeCol + mistCol, 0.5);
            }
        }

        // Apply Cloud Shadows on top of Ground
        if (uShowCloudShadow) {
            groundColor = mix(groundColor, groundColor * 0.35, 1.0 - shadowTransmittance);
        }

        // Overlay Cloud Cover (using actual coverage map) on top of Shadows
        combinedColor = groundColor;
        if (uShowCloudCover) {
            float cloudAlpha = cloudData.r * 0.75; // Use raw coverage map!
            combinedColor = mix(combinedColor, vec3(0.95, 0.95, 0.95), cloudAlpha);
        }

        // Overlay Wind flow on top of EVERYTHING
        if (uShowWind && inLbmBounds) {
            float v = length(wind.xz) / 25.0;
            vec2 windDir = normalize(wind.xz + vec2(1e-5));
            float flow = sin(dot(worldXZ, windDir) * 0.005 - uTime * 5.0) * 0.5 + 0.5;

            // Beautiful, high-contrast, neon cyan-blue wind flow lines
            vec3 windCol = vec3(0.1, 0.65, 1.0) * (0.6 + 0.4 * flow);
            float alpha = clamp(v * 0.6, 0.0, 0.8);
            combinedColor = mix(combinedColor, windCol, alpha);
        }
    }
    else if (uSelectedLayer == 1) { // Raw Cloud Map (the colorful baked weather map)
        combinedColor = cloudData.rgb;
    }
    else if (uSelectedLayer == 2) { // Cloud Effective Coverage
        combinedColor = vec3(cloudData.r);
    }
    else if (uSelectedLayer == 3) { // Deep Opacity Map (raw optical depth at layer 7)
        combinedColor = vec3(shadowDensity * 0.01); // scale to visible range
    }
    else if (uSelectedLayer == 4) { // LBM Simulation
        combinedColor = vec3(0.0);
        vec2 lbmLocalPos = (worldXZ - vec2(uLbmGridOrigin.x, uLbmGridOrigin.y) * uLbmSpacing);
        vec2 lbmUV = lbmLocalPos / (vec2(uLbmGridSize) * uLbmSpacing);

        if (lbmUV.x >= 0.0 && lbmUV.x <= 1.0 && lbmUV.y >= 0.0 && lbmUV.y <= 1.0) {
            vec4 wind = texture(uLbmWindTex, lbmUV);
            vec4 scalars = texture(uLbmScalarTex, lbmUV);
            vec4 aerosols = texture(uLbmAerosolTex, lbmUV);

            if (uShowTemperature) {
                float tempC = scalars.x - 273.15;
                vec3 tempColor = mix(vec3(0.0, 0.5, 1.0), vec3(1.0, 0.1, 0.0), clamp((tempC + 10.0) / 50.0, 0.0, 1.0));
                combinedColor = mix(combinedColor, tempColor, 0.7);
            }
            if (uShowHumidity) {
                vec3 humidColor = mix(vec3(0.8, 0.8, 0.5), vec3(0.1, 0.5, 0.8), scalars.y);
                combinedColor = mix(combinedColor, humidColor, 0.7);
            }
            if (uShowPressure) {
                float pressFactor = clamp((scalars.z - 980.0) / 60.0, 0.0, 1.0);
                vec3 pressColor = mix(vec3(0.5, 0.0, 0.5), vec3(0.9, 0.9, 0.1), pressFactor);
                combinedColor = mix(combinedColor, pressColor, 0.7);
            }
            if (uShowWind) {
                float v = length(wind.xz) / 20.0;
                vec2 windDir = normalize(wind.xz + vec2(1e-5));
                float flow = sin(dot(worldXZ, windDir) * 0.005 - uTime * 5.0) * 0.5 + 0.5;
                combinedColor += vec3(0.0, v * 0.5 + flow * v * 0.3, v + flow * v * 0.3);
            }
            if (uShowAerosol) {
                vec3 dustCol   = vec3(0.7, 0.4, 0.15) * clamp(aerosols.r * 15.0, 0.0, 1.0);
                vec3 pollenCol = vec3(0.5, 0.8, 0.1)  * clamp(aerosols.g * 15.0, 0.0, 1.0);
                vec3 smokeCol  = vec3(0.3, 0.3, 0.35) * clamp(aerosols.b * 15.0, 0.0, 1.0);
                vec3 mistCol   = vec3(0.4, 0.7, 0.95) * clamp(aerosols.a * 15.0, 0.0, 1.0);
                combinedColor += dustCol + pollenCol + smokeCol + mistCol;
            }
        } else {
            combinedColor = vec3(0.1, 0.0, 0.0); // Outside simulation bounds
        }
    }
    else if (uSelectedLayer == 5) { // Terrain Height
        if (hasTerrain && surface.height > -9000.0) {
            float normHeight = clamp((surface.height + 100.0) / 2000.0, 0.0, 1.0);
            combinedColor = vec3(normHeight);
        } else {
            combinedColor = vec3(0.0);
        }
    }
    else if (uSelectedLayer == 6) { // Terrain Color
        if (hasTerrain && surface.height > -9000.0) {
            combinedColor = terrainShaded;
        } else {
            combinedColor = vec3(0.1, 0.1, 0.15);
        }
    }
    else if (uSelectedLayer == 7) { // Baked Wind
        combinedColor = vec3(0.0);
        vec2 lbmLocalPos = (worldXZ - vec2(uLbmGridOrigin.x, uLbmGridOrigin.y) * uLbmSpacing);
        vec2 lbmUV = lbmLocalPos / (vec2(uLbmGridSize) * uLbmSpacing);

        if (lbmUV.x >= 0.0 && lbmUV.x <= 1.0 && lbmUV.y >= 0.0 && lbmUV.y <= 1.0) {
            vec4 wind = texture(uBakedWindTex, lbmUV);
            if (uShowWind) {
                float v = length(wind.xz) / 20.0;
                vec2 windDir = normalize(wind.xz + vec2(1e-5));
                float flow = sin(dot(worldXZ, windDir) * 0.005 - uTime * 5.0) * 0.5 + 0.5;
                combinedColor = vec3(0.0, v * 0.5 + flow * v * 0.3, v + flow * v * 0.3);
            }
        } else {
            combinedColor = vec3(0.1, 0.0, 0.0); // Outside bounds
        }
    }

    // --- 4. Overlays for LBM and other Phenomenon in Combined View ---
    // (Handled directly inside uSelectedLayer == 0 section above to ensure perfect blending order)

    // --- 5. Camera Indicator ---
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
