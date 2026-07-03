#version 460 core

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform vec2      RESOLUTION;

uniform bool      uAutofocus;
uniform float     uManualFocusDistance;
uniform vec2      uFocalPointOffset;
uniform float     uMinFocusDistance;
uniform float     uMaxFocusDistance;

uniform float     uFocusScale;
uniform float     uBlurSize;
uniform mat4      uInvProjection;

in vec2 TexCoords;
out vec4 FragColor;

// float getLinearDepth(vec2 uv) {
//     float depth = texture(depthTexture, uv).r;
//     vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
//     vec4 viewSpacePos = uInvProjection * clipSpacePos;
//     return -viewSpacePos.z / viewSpacePos.w;
// }

float getLinearDepth(vec2 uv) {
    float depth = texture(depthTexture, uv).r;
    if (depth > 0.99999) return 50000.0;

    vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpacePos = uInvProjection * clipSpacePos;
    return -viewSpacePos.z / viewSpacePos.w;
}

// Sample depth with a small kernel for more stable autofocus
float getStableFocusDepth(vec2 uv) {
    float d = 0.0;
    vec2 off = 1.0 / RESOLUTION;
    d += getLinearDepth(uv);
    d += getLinearDepth(uv + vec2(off.x, 0.0));
    d += getLinearDepth(uv + vec2(-off.x, 0.0));
    d += getLinearDepth(uv + vec2(0.0, off.y));
    d += getLinearDepth(uv + vec2(0.0, -off.y));
    return d / 5.0;
}



// Custom sampleDoF that handles autofocus and linear depth
vec3 sampleDoF_enhanced(sampler2D tex, sampler2D texDepth, vec2 st, float focusPoint, float focusScale) {
    float actualFocusDist = focusPoint;
    if (uAutofocus) {
        vec2 focusUV = vec2(0.5) + uFocalPointOffset;
        actualFocusDist = getLinearDepth(focusUV);
        actualFocusDist = clamp(actualFocusDist, uMinFocusDistance, uMaxFocusDistance);
    }

    float currentDepth = getLinearDepth(st);

    // CoC calculation using linear depth
    float coc = clamp((1.0/actualFocusDist - 1.0/currentDepth) * focusScale, -1.0, 1.0);
    float blurSize = abs(coc) * uBlurSize;

    vec2 pixelSize = 1.0 / RESOLUTION;
    vec3 color = texture(tex, st).rgb;

    vec3 colorSum = vec3(0.0);
    float totalWeight = 0.0;

    // Always start with the center sample to guarantee a baseline
    colorSum += color;
    totalWeight += 1.0;

    const int SAMPLES = 32;
    const float GOLDEN_ANGLE = 2.39996323;

    for (int i = 0; i < SAMPLES; i++) {
        float r = sqrt(float(i) + 0.5) / sqrt(float(SAMPLES));
        float theta = float(i) * GOLDEN_ANGLE;

        float currentSampleRadius = r * blurSize;
        vec2 tc = TexCoords + vec2(cos(theta), sin(theta)) * currentSampleRadius * pixelSize;

        float sampleDepth = getLinearDepth(tc);
        float sampleCoc = clamp((1.0/actualFocusDist - 1.0/sampleDepth) * uFocusScale, -1.0, 1.0);
        float sampleBlurRadius = abs(sampleCoc) * uBlurSize;

        // Weighting:
        // Sharp background shouldn't bleed into blurred foreground
        float weight = 1.0;
        if (sampleDepth > currentDepth) {
            weight = smoothstep(currentSampleRadius + 0.5, currentSampleRadius - 0.5, sampleBlurRadius);
        }
        vec3 sampleColor = texture(tex, tc).rgb;
        colorSum += sampleColor * weight;
        totalWeight += weight;
    }

    return colorSum / totalWeight;
}

void main() {
    FragColor = vec4(sampleDoF_enhanced(screenTexture, depthTexture, TexCoords, uManualFocusDistance, uFocusScale), 1.0);
}
