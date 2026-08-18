#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec3 uCameraPos;
uniform vec3 uCameraFront;
uniform float uViewScale;
uniform vec2 uViewOffset;

uniform int uSelectedLayer; // 0: Combined, 1: Raw Cloud Map, 2: Cloud Effective Coverage, 3: Deep Opacity Map, 4: LBM Simulation, 5: Terrain Height, 6: Terrain Color, 7: Baked Wind
uniform bool uShowWind;
uniform bool uShowTemperature;
uniform bool uShowHumidity;
uniform bool uShowPressure;
uniform bool uShowAerosol;
uniform bool uShowCloudCover;
uniform bool uShowCloudHeight;
uniform bool uShowCloudShadow;

uniform float uCloudCoverage;
uniform float uCloudAltitude;
uniform float uCloudThickness;

uniform sampler2D uCloudWeatherTex;
uniform sampler2D uBakedWindTex;
uniform sampler2D uBakedWindUvTex;
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

    // --- 1. Terrain Layer (Universal Base) ---
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
    // cloudData: R=finalCoverage, G=height, B=thickness, A=density

    // Project for Cloud Shadow Map (sampler2DArray)
    vec4 lightSpacePos = uCloudShadowMatrix * vec4(worldXZ.x, 0.0, worldXZ.y, 1.0);
    vec2 shadowUV = lightSpacePos.xy * 0.5 + 0.5;

    float shadowDensity = 0.0;
    float shadowTransmittance = 1.0;

    if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
        shadowDensity = textureLod(uCloudShadowTex, vec3(shadowUV, 7.0), 1.5).r;
        shadowTransmittance = exp(-shadowDensity * 0.001 * 2.0 / max(0.001, uWorldScale));
    }

    // Dynamic cloud thresholding/effective coverage
    float effectiveCoverage = clamp(cloudData.r + (uCloudCoverage * 2.0 - 1.0), 0.0, 1.0);

    // --- 3. Compute Base Diagnostic Color ---
    vec3 baseColor = terrainShaded;

    if (uSelectedLayer == 1) { // Raw Cloud Map (the colorful baked weather map)
        baseColor = cloudData.rgb;
    }
    else if (uSelectedLayer == 2) { // Cloud Effective Coverage
        // Blend effective coverage over the shaded terrain so you can see where clouds form relative to terrain!
        baseColor = mix(terrainShaded, vec3(1.0, 1.0, 1.0), effectiveCoverage * 0.8);
    }
    else if (uSelectedLayer == 3) { // Deep Opacity Map (raw optical depth at layer 7)
        // Show optical depth as a red-hot thermal map or raw gray on terrain
        float depth = clamp(shadowDensity * 0.01, 0.0, 1.0);
        baseColor = mix(terrainShaded, vec3(1.0, 0.2, 0.1), depth);
    }
    else if (uSelectedLayer == 4) { // LBM Simulation
        // Start with shaded terrain and apply LBM bounds base if in bounds
        vec2 lbmLocalPos = (worldXZ - vec2(uLbmGridOrigin.x, uLbmGridOrigin.y) * uLbmSpacing);
        vec2 lbmUV = lbmLocalPos / (vec2(uLbmGridSize) * uLbmSpacing);

        if (lbmUV.x >= 0.0 && lbmUV.x <= 1.0 && lbmUV.y >= 0.0 && lbmUV.y <= 1.0) {
            // Give LBM bounds a subtle outline or base tint
            baseColor = mix(terrainShaded, terrainShaded * 1.1, 0.1);
        }
    }
    else if (uSelectedLayer == 5) { // Terrain Height
        if (hasTerrain && surface.height > -9000.0) {
            float normHeight = clamp((surface.height + 100.0) / 2000.0, 0.0, 1.0);
            baseColor = vec3(normHeight);
        } else {
            baseColor = vec3(0.0);
        }
    }
    else if (uSelectedLayer == 6) { // Terrain Color
        baseColor = terrainShaded;
    }
    else if (uSelectedLayer == 7) { // Baked Wind Base
        baseColor = terrainShaded;
    }

    // --- 4. Apply Unified Layer Overlays (Temperature, Humidity, Aerosol) ---
    // These diagnostic overlays are toggleable via checkboxes on top of ANY selected base map!
    vec2 lbmLocalPos = (worldXZ - vec2(uLbmGridOrigin.x, uLbmGridOrigin.y) * uLbmSpacing);
    vec2 lbmUV = lbmLocalPos / (vec2(uLbmGridSize) * uLbmSpacing);
    bool inLbmBounds = (lbmUV.x >= 0.0 && lbmUV.x <= 1.0 && lbmUV.y >= 0.0 && lbmUV.y <= 1.0);

    if (inLbmBounds) {
        vec4 scalars = texture(uLbmScalarTex, lbmUV);
        vec4 aerosols = texture(uLbmAerosolTex, lbmUV);

        if (uShowTemperature) {
            float tempC = scalars.x - 273.15;
            vec3 tempColor = mix(vec3(0.0, 0.2, 0.8), vec3(0.8, 0.1, 0.0), clamp((tempC + 10.0) / 50.0, 0.0, 1.0));
            baseColor = mix(baseColor, tempColor, 0.35);
        }
        if (uShowHumidity) {
            vec3 humidColor = mix(vec3(0.8, 0.8, 0.5), vec3(0.1, 0.4, 0.7), scalars.y);
            baseColor = mix(baseColor, humidColor, 0.35);
        }
        if (uShowAerosol) {
            vec3 dustCol   = vec3(0.7, 0.4, 0.15) * clamp(aerosols.r * 15.0, 0.0, 1.0);
            vec3 pollenCol = vec3(0.5, 0.8, 0.1)  * clamp(aerosols.g * 15.0, 0.0, 1.0);
            vec3 smokeCol  = vec3(0.3, 0.3, 0.35) * clamp(aerosols.b * 15.0, 0.0, 1.0);
            vec3 mistCol   = vec3(0.4, 0.7, 0.95) * clamp(aerosols.a * 15.0, 0.0, 1.0);
            baseColor = mix(baseColor, baseColor + dustCol + pollenCol + smokeCol + mistCol, 0.5);
        }
    }

    // --- 5. Apply Cloud Shadows on top of the ground/diagnostic layers ---
    if (uShowCloudShadow) {
        baseColor = mix(baseColor, baseColor * 0.35, 1.0 - shadowTransmittance);
    }

    // --- 6. Overlay Cloud Cover on top of Shadows ---
    if (uShowCloudCover) {
        float cloudAlpha = effectiveCoverage * 0.75; // Uses the thresholded effective coverage!
        baseColor = mix(baseColor, vec3(0.95, 0.95, 0.95), cloudAlpha);
    }

    // --- 7. Overlay Wind Flow on top of EVERYTHING ---
    if (uShowWind && inLbmBounds) {
        // Select which wind texture to use (LBM simulation mode uses raw LBM wind, otherwise baked wind)
        bool useRawLbmWind = (uSelectedLayer == 4);
        vec4 wind;
        if (useRawLbmWind) {
            wind = texture(uLbmWindTex, lbmUV);
        } else {
            wind = texture(uBakedWindTex, lbmUV);
        }

        float v = length(wind.xz) / 25.0;
        vec2 windDir = normalize(wind.xz + vec2(1e-5));
        float flow = sin(dot(worldXZ, windDir) * 0.005 - uTime * 5.0) * 0.5 + 0.5;

        // Custom styling for wind lines: Neon Cyan/Blue for Baked Wind, Neon Green/Yellow for Raw LBM
        vec3 windCol = useRawLbmWind ? vec3(0.1, 1.0, 0.4) * (0.6 + 0.4 * flow)
                                     : vec3(0.1, 0.65, 1.0) * (0.6 + 0.4 * flow);
        float alpha = clamp(v * 0.6, 0.0, 0.8);
        baseColor = mix(baseColor, windCol, alpha);
    }

    vec3 combinedColor = baseColor;

    // --- 8. Camera Indicator ---
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
