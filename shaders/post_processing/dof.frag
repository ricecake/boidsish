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
    if (depth > 0.99999) return 50000.0;

    vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpacePos = uInvProjection * clipSpacePos;
    if (abs(viewSpacePos.w) < 0.0001) return 50000.0;
    return -viewSpacePos.z / viewSpacePos.w;
}

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

vec4 safe_sample(vec2 uv) {
    vec4 c = texture(screenTexture, uv);
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
    float blurSize = abs(coc) * uBlurSize;

    if (blurSize < 0.05) {
        FragColor = safe_sample(TexCoords);
        return;
    }

    vec2 pixelSize = 1.0 / RESOLUTION;
    vec4 colorSum = safe_sample(TexCoords);
    float totalWeight = 1.0;

    float radius = 0.5;
    const float GOLDEN_ANGLE = 2.39996323;

    // Standard weighted accumulation loop (more stable than incremental average for HDR)
    for (float ang = 0.0; radius < blurSize; ang += GOLDEN_ANGLE) {
        vec2 tc = TexCoords + vec2(cos(ang), sin(ang)) * pixelSize * radius;
        float sampleDepth = getLinearDepth(tc);

        float sampleCoc = clamp((1.0/focusDist - 1.0/sampleDepth) * uFocusScale, -1.0, 1.0);
        float sampleBlurSize = abs(sampleCoc) * uBlurSize;

        // Bleeding prevention: don't let far objects blur over near ones
        if (sampleDepth > centerDepth)
            sampleBlurSize = clamp(sampleBlurSize, 0.0, blurSize * 2.0);

        float weight = smoothstep(radius - 0.5, radius + 0.5, sampleBlurSize);
        vec4 sampleColor = safe_sample(tc);

        colorSum += sampleColor * weight;
        totalWeight += weight;

        radius += 0.5 / radius;
    }

    FragColor = colorSum / max(totalWeight, 0.0001);
}
