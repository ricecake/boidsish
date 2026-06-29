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

float getLinearDepth(vec2 uv) {
    float depth = texture(depthTexture, uv).r;
    if (depth >= 0.999) return 100000.0;

    vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpacePos = uInvProjection * clipSpacePos;

    float w = viewSpacePos.w;
    if (abs(w) < 0.000001) return 100000.0;

    float linearZ = -viewSpacePos.z / w;
    return clamp(linearZ, 0.1, 100000.0);
}

float getStableFocusDepth(vec2 uv) {
    vec2 off = 1.0 / RESOLUTION;
    float d = getLinearDepth(uv);
    d += getLinearDepth(uv + vec2(off.x, 0.0));
    d += getLinearDepth(uv + vec2(-off.x, 0.0));
    d += getLinearDepth(uv + vec2(0.0, off.y));
    d += getLinearDepth(uv + vec2(0.0, -off.y));
    return d * 0.2;
}

vec4 fetch_safe(sampler2D tex, vec2 uv) {
    vec4 c = texture(tex, uv);
    if (any(isnan(c)) || any(isinf(c))) return vec4(0.0);
    return max(c, vec4(0.0));
}

void main() {
    float focusDist = uManualFocusDistance;
    if (uAutofocus) {
        vec2 focusUV = vec2(0.5) + uFocalPointOffset;
        focusDist = getStableFocusDepth(focusUV);
        focusDist = clamp(focusDist, uMinFocusDistance, uMaxFocusDistance);
    }

    float centerDepth = getLinearDepth(TexCoords);
    float coc = clamp((1.0/focusDist - 1.0/centerDepth) * uFocusScale, -1.0, 1.0);
    float blurRadius = abs(coc) * uBlurSize;

    if (blurRadius < 0.2) {
        FragColor = fetch_safe(screenTexture, TexCoords);
        return;
    }

    vec2 pixelSize = 1.0 / RESOLUTION;
    vec4 colorSum = vec4(0.0);
    float totalWeight = 0.0;

    // Always start with the center sample to guarantee a baseline
    vec4 centerColor = fetch_safe(screenTexture, TexCoords);
    colorSum += centerColor;
    totalWeight += 1.0;

    const int SAMPLES = 32;
    const float GOLDEN_ANGLE = 2.39996323;

    for (int i = 0; i < SAMPLES; i++) {
        float r = sqrt(float(i) + 0.5) / sqrt(float(SAMPLES));
        float theta = float(i) * GOLDEN_ANGLE;

        float currentSampleRadius = r * blurRadius;
        vec2 tc = TexCoords + vec2(cos(theta), sin(theta)) * currentSampleRadius * pixelSize;

        float sampleDepth = getLinearDepth(tc);
        float sampleCoc = clamp((1.0/focusDist - 1.0/sampleDepth) * uFocusScale, -1.0, 1.0);
        float sampleBlurRadius = abs(sampleCoc) * uBlurSize;

        // Weighting:
        // Sharp background shouldn't bleed into blurred foreground
        float weight = 1.0;
        if (sampleDepth > centerDepth) {
            weight = smoothstep(currentSampleRadius + 0.5, currentSampleRadius - 0.5, sampleBlurRadius);
        }

        vec4 sampleColor = fetch_safe(screenTexture, tc);
        colorSum += sampleColor * weight;
        totalWeight += weight;
    }

    FragColor = colorSum / totalWeight;
}
