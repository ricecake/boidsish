#version 460 core
out vec4 FragColor;

#include "lygia/generative/snoise.glsl"
#include "visual_effects.glsl"

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

// Wind Blur controls
uniform float u_WindAngle;
uniform float u_WindSpeed;
uniform float u_WindBlurScale;
uniform float u_WindGustFrequency;
uniform float u_WindGustStrength;

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
vec2 hash22(vec2 p) {
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
            vec2 o = hash22(n + g);
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
            vec2 o = hash22(n + g);
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
    // Generate noise coords stretched vertically in world-space Y
    // Pinned to world space so lines remain vertical regardless of camera rotation
    vec3 heatNoiseCoord = vec3(worldPos.x, worldPos.y * 0.1, worldPos.z) * u_HeatScale + vec3(0.0, -time * u_HeatSpeed, 0.0);
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
    // EFFECT 2: Wind-driven Directional Blur
    // ==========================================
    // Compute 3D wind velocity vector in world-space
    vec3 windWorldDir = vec3(sin(radians(u_WindAngle)), 0.0, -cos(radians(u_WindAngle)));
    // Modulate speed by the VisualEffects UBO's wind_strength
    float windSpeedVal = u_WindSpeed * (1.0 + wind_strength);
    vec3 windWorldVel = windWorldDir * windSpeedVal;

    // Transform to view space
    vec4 windViewVel = viewMatrix * vec4(windWorldVel, 0.0);

    // Perform directional blur along projected screen wind vector
    vec2 windScreenVec = windViewVel.xy;

    // Add gustiness over time
    float gust = 1.0 + u_WindGustStrength * snoise(vec3(0.0, 0.0, time * u_WindGustFrequency));
    vec2 blurVec = windScreenVec * u_WindBlurScale * gust;

    vec4 blurredColor = vec4(0.0);
    float totalWeight = 0.0;
    const int numTaps = 8;
    for (int i = 0; i < numTaps; ++i) {
        float t = float(i) / float(numTaps - 1) - 0.5; // [-0.5, 0.5]
        vec2 tapOffset = blurVec * t;
        float weight = 1.0 - abs(t) * 2.0; // Triangle filter
        blurredColor += texture(sceneTexture, deformedUVs + tapOffset) * weight;
        totalWeight += weight;
    }
    vec3 finalSceneColor = blurredColor.rgb / totalWeight;

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
