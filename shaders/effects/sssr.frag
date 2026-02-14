#version 420 core

layout(location = 0) out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D normalTexture;
uniform sampler2D pbrTexture;
uniform sampler2D hizTexture;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 invProjection;
uniform mat4 invView;

uniform vec3 cameraPos;
uniform float time;
uniform uint frameCount;
uniform float worldScale;

// SSSR parameters
const int   MAX_STEPS = 64;
const int   HI_Z_MAX_MIP = 7;
const float THICKNESS = 0.5;

// GGX and Hammersley helper functions
const float PI = 3.14159265359;

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// Hierarchical Z-Buffer Ray Marching
bool HiZ_RayMarch(vec3 origin, vec3 dir, out vec3 hitPos, out vec2 hitUV) {
    vec3 rayEnd = origin + dir * (1000.0 * worldScale);

    vec4 h0 = projection * vec4(origin, 1.0);
    vec4 h1 = projection * vec4(rayEnd, 1.0);

    vec3 screenOrigin = (h0.xyz / h0.w) * 0.5 + 0.5;
    vec3 screenEnd = (h1.xyz / h1.w) * 0.5 + 0.5;

    vec3 screenDir = normalize(screenEnd - screenOrigin);

    // Conservative step logic using Hi-Z
    vec3 currentPos = screenOrigin;
    int mip = 0;

    for(int i = 0; i < MAX_STEPS; i++) {
        vec2 uv = currentPos.xy;
        if(uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return false;

        float cellDepth = textureLod(hizTexture, uv, mip).r;

        if(currentPos.z > cellDepth) {
            if(mip == 0) {
                // Potential hit, check thickness
                if(abs(currentPos.z - cellDepth) < THICKNESS / (1000.0 * worldScale)) {
                    hitPos = currentPos;
                    hitUV = uv;
                    return true;
                }
                // Step forward
                currentPos += screenDir * (1.0 / 1024.0);
            } else {
                // Go down a level
                mip--;
            }
        } else {
            // No hit, go up a level and take a larger step
            mip = min(mip + 1, HI_Z_MAX_MIP);
            currentPos += screenDir * (exp2(mip) / 512.0);
        }
    }

    return false;
}

void main() {
    float depth = texture(depthTexture, TexCoords).r;
    if (depth >= 1.0) {
        FragColor = texture(sceneTexture, TexCoords);
        return;
    }

    vec3 normal = texture(normalTexture, TexCoords).rgb;
    vec2 pbr = texture(pbrTexture, TexCoords).rg;
    float roughness = pbr.r;
    float metallic = pbr.g;

    // Only process reflective surfaces
    if (roughness > 0.8) {
        FragColor = texture(sceneTexture, TexCoords);
        return;
    }

    // Reconstruct world position
    vec4 clipPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos4 = invProjection * clipPos;
    vec3 viewPos = viewPos4.xyz / viewPos4.w;
    vec3 worldPos = (invView * vec4(viewPos, 1.0)).xyz;

    vec3 V = normalize(cameraPos - worldPos);
    vec3 N = normalize(normal);

    // Stochastic Sampling using Hammersley and GGX Importance Sampling
    uint sampleIdx = frameCount % 16u; // Cycle through 16 samples for temporal accumulation
    vec2 xi = Hammersley(sampleIdx, 16u);
    vec3 H = ImportanceSampleGGX(xi, N, roughness);
    vec3 R = reflect(-V, H);

    // Fade reflections near the edge of the screen
    vec2 dUV = abs(TexCoords - 0.5) * 2.0;
    float edgeFade = 1.0 - max(smoothstep(0.8, 1.0, dUV.x), smoothstep(0.8, 1.0, dUV.y));

    // Hi-Z Ray Marching
    vec3 hitPos;
    vec2 hitUV;

    vec3 originVS = viewPos;
    vec3 dirVS = normalize(reflect(normalize(viewPos), (view * vec4(H, 0.0)).xyz));

    bool hit = HiZ_RayMarch(originVS, dirVS, hitPos, hitUV);

    vec3 reflectionColor = texture(sceneTexture, hitUV).rgb;
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    // Base reflectivity for dielectrics is around 0.04
    float reflectionStrength = mix(0.04, 1.0, metallic);
    reflectionStrength = mix(reflectionStrength, 1.0, fresnel) * edgeFade;

    if (hit) {
        vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
        FragColor = vec4(mix(sceneColor, reflectionColor, reflectionStrength), 1.0);
    } else {
        FragColor = texture(sceneTexture, TexCoords);
    }
}
