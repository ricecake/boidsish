#version 460 core
out vec4 FragColor;

#include "lygia/generative/snoise.glsl"
#include "lygia/generative/curl.glsl"
#include "visual_effects.glsl"
#include "helpers/wind.glsl"

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D velocityTexture;
uniform sampler2D normalTexture;
uniform sampler2D albedoTexture;
uniform float     time;

// Matrices and camera info for world space reconstruction
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 invView;
uniform mat4 invProjection;
uniform vec3 cameraPos;

// Heat Shimmer controls
uniform float u_HeatStrength;
uniform float u_HeatScale;
uniform float u_HeatSpeed;
uniform float u_HeatWidth;
uniform float u_HeatHeight;

// Wind Blur controls
uniform float u_WindAngle;
uniform float u_WindSpeed;
uniform float u_WindBlurScale;
uniform float u_WindGustFrequency;
uniform float u_WindGustStrength;
uniform float u_WindRoughenStrength;
uniform float u_WindStreakDecay;
uniform float u_WindTintStrength;

// Ice Crystal controls
uniform bool  u_UseManualIceCoverage;
uniform float u_IceCoverage;
uniform float u_IceScale;
uniform float u_IceEdgeWidth;
uniform vec3  u_IceColor;

const float near = 0.1;
const float far = 3000.0;

float LinearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

// Simple hash function for Voronoi
vec2 weather_hash22(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453123);
}

// Distance-to-edge Voronoi for sharp crystal frost boundaries
float voronoiDistanceToEdge(vec2 x) {
    vec2 n = floor(x);
    vec2 f = fract(x);

    vec2 mg, mr;
    float md = 8.0;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = weather_hash22(n + g);
            // Animate cells slightly with time to make the frost look "alive"
            o = 0.5 + 0.5 * sin(time * 0.15 + 6.2831 * o);
            vec2 r = g + o - f;
            float d = dot(r, r);

            if (d < md) {
                md = d;
                mr = r;
                mg = g;
            }
        }
    }

    md = 8.0;
    for (int j = -2; j <= 2; ++j) {
        for (int i = -2; i <= 2; ++i) {
            vec2 g = mg + vec2(float(i), float(j));
            vec2 o = weather_hash22(n + g);
            o = 0.5 + 0.5 * sin(time * 0.15 + 6.2831 * o);
            vec2 r = g + o - f;

            if (dot(mr - r, mr - r) > 0.00001) {
                md = min(md, dot(0.5 * (mr + r), normalize(r - mr)));
            }
        }
    }
    return md;
}

void main() {
    // 1. Reconstruct world space position
    float depth = texture(depthTexture, TexCoords).r;
    float linDepth = LinearizeDepth(depth);

    // Clamp depth slightly for sky to allow background heat shimmer
    float depthForReconstruction = min(depth, 0.9999);
    vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, depthForReconstruction * 2.0 - 1.0, 1.0);
    vec4 viewPos = invProjection * ndcPos;
    viewPos /= viewPos.w;
    vec4 worldPos = invView * viewPos;

    float distToCam = length(worldPos.xyz - cameraPos);

    // ==========================================
    // EFFECT 1: Heat Shimmer (Heat Lines)
    // ==========================================
    // Generate noise coords stretched vertically and horizontally in world-space
    // Use u_HeatWidth and u_HeatHeight to sculpt wave proportions (emphasizing width)
    vec3 heatNoiseCoord = vec3(worldPos.x * u_HeatWidth, worldPos.y * u_HeatHeight, worldPos.z * u_HeatWidth) * u_HeatScale + vec3(0.0, -time * u_HeatSpeed, 0.0);
    float n1 = snoise(heatNoiseCoord);
    float n2 = snoise(heatNoiseCoord * 2.15 + vec3(1.1, 0.0, 0.8));
    float shimmerVal = n1 * 0.7 + n2 * 0.3;

    // Amplitude scales with distance to create natural perspective mirage look
    // Cap at a reasonable distance (e.g. 500 meters)
    float heatDepthFactor = u_HeatStrength * clamp(distToCam * 0.002, 0.0, 1.0);

    // Displace horizontally in screen-space
    vec2 heatOffset = vec2(shimmerVal, snoise(heatNoiseCoord + vec3(7.0, 13.0, 23.0)) * 0.15) * heatDepthFactor * 0.01;

    vec2 deformedUVs = TexCoords + heatOffset;
    deformedUVs = clamp(deformedUVs, 0.0, 1.0);

    // ==========================================
    // EFFECT 2: Wind-driven Trailing Streak Blur
    // ==========================================
    // Read wind velocity from the wind weather texture at this world position, with manual fallback
    vec3 windWorldVel;
    if (u_windOriginSize.y > 0) {
        vec3 localWind = getWindAtPosition(worldPos.xyz);
        windWorldVel = localWind * u_WindSpeed;
    } else {
        // Fallback to manual angle and speed, scaling with UBO's wind_strength
        vec3 windWorldDir = vec3(sin(radians(u_WindAngle)), 0.0, -cos(radians(u_WindAngle)));
        windWorldVel = windWorldDir * u_WindSpeed * (1.0 + wind_strength);
    }
    windWorldVel *= smoothstep(0.80, 1.0, dot(windWorldVel, curl(vec3(worldPos.x, time*0.5, worldPos.z))));


    // Transform 3D wind velocity to view space
    vec4 windViewVel = viewMatrix * vec4(windWorldVel, 0.0);

    // Perform directional streak blur along projected screen wind vector
    vec2 windScreenVec = windViewVel.xy;

    // Add gustiness over time
    float gust = 1.0 + u_WindGustStrength * snoise(vec3(0.0, 0.0, time * u_WindGustFrequency));
    vec2 blurVec = windScreenVec * u_WindBlurScale * gust;

    vec3 blurAccum = vec3(0.0);
    float weightAccum = 0.0;
    const int numTaps = 12; // High-quality streaks

    for (int i = 0; i < numTaps; ++i) {
        float t = float(i) / float(numTaps - 1); // [0.0, 1.0]

        // Jitter/roughen sample coordinates with high-frequency noise for fuzzed/wind-swept look
        vec2 jitter = vec2(
            snoise(vec3(TexCoords * 400.0, time * 25.0 + float(i))),
            snoise(vec3(TexCoords * 400.0 + vec2(19.0, 29.0), time * 25.0 + float(i)))
        ) * u_WindRoughenStrength;

        // Trail off exponentially along the wind vector
        vec2 tapOffset = blurVec * t + jitter * 0.003;
        float weight = exp(-t * u_WindStreakDecay); // Exponential decay

        blurAccum += texture(sceneTexture, deformedUVs - tapOffset).rgb * weight;
        weightAccum += weight;
    }
    vec3 finalSceneColor = blurAccum / weightAccum;

    // Tint the scene white/grey based on wind speed to simulate wind-swept mist/dust
    float windStrengthVal = length(windWorldVel);
    vec3 tintColor = vec3(0.92, 0.94, 0.95); // Frosty wind-swept grey-white tint
    float tintFactor = clamp(windStrengthVal * u_WindTintStrength * 0.05, 0.0, 0.85);
    finalSceneColor = mix(finalSceneColor, tintColor, tintFactor);

    // ==========================================
    // EFFECT 3: Temperature-driven Ice Crystals
    // ==========================================
    float iceCoverageFactor = u_UseManualIceCoverage ? u_IceCoverage : smoothstep(273.15, 263.15, temperature);

    vec3 outputColor = finalSceneColor;

    if (iceCoverageFactor > 0.001) {
        // Calculate screen edge mask (vignette shape)
        float edgeDist = length(TexCoords - 0.5) * 1.414; // Corner is 1.0
        float edgeMask = smoothstep(1.0 - u_IceEdgeWidth, 1.0, edgeDist);

        // Warp Voronoi coordinates with noise for organic crystalline look
        vec2 warp = vec2(
            snoise(vec3(TexCoords * u_IceScale, time * 0.05)),
            snoise(vec3(TexCoords * u_IceScale + vec2(17.0, 23.0), time * 0.05))
        );
        vec2 warpedCoords = TexCoords * u_IceScale + warp * 0.4;

        float voronoiDist = voronoiDistanceToEdge(warpedCoords);

        // Compute frost needle pattern
        float crystalLine = 1.0 - smoothstep(0.0, 0.07, voronoiDist);
        float crystalFill = smoothstep(0.05, 0.4, voronoiDist);
        float frostVal = max(crystalLine * 0.9, crystalFill * 0.15) * edgeMask;

        // Threshold with coverage to grow crystals from the edge
        float finalFrost = smoothstep(1.0 - iceCoverageFactor, 1.05 - iceCoverageFactor, frostVal + snoise(vec3(TexCoords * u_IceScale, time * 0.12)) * 0.15);

        // Soft frost glow at the edge
        float edgeGlow = smoothstep(1.0 - u_IceEdgeWidth * 0.5, 1.0, edgeDist) * iceCoverageFactor * 0.5;
        float iceMask = clamp(finalFrost + edgeGlow, 0.0, 1.0);

        outputColor = mix(finalSceneColor, u_IceColor, iceMask);
    }

    FragColor = vec4(outputColor, 1.0);
}
