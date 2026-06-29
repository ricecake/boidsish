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
        FragColor = texture(screenTexture, TexCoords);
        return;
    }

    vec2 pixelSize = 1.0 / RESOLUTION;
    vec4 color = texture(screenTexture, TexCoords);

    float tot = 1.0;
    float radius = 0.5;
    const float GOLDEN_ANGLE = 2.39996323;

    for (float ang = 0.0; radius < blurSize; ang += GOLDEN_ANGLE) {
        vec2 tc = TexCoords + vec2(cos(ang), sin(ang)) * pixelSize * radius;
        float sampleDepth = getLinearDepth(tc);

        float sampleCoc = clamp((1.0/focusDist - 1.0/sampleDepth) * uFocusScale, -1.0, 1.0);
        float sampleBlurSize = abs(sampleCoc) * uBlurSize;

        if (sampleDepth > centerDepth)
            sampleBlurSize = clamp(sampleBlurSize, 0.0, blurSize * 2.0);

        float pct = smoothstep(radius - 0.5, radius + 0.5, sampleBlurSize);
        vec4 sampleColor = texture(screenTexture, tc);

        // Protect against NaNs/Infs in the source texture
        if (any(isnan(sampleColor)) || any(isinf(sampleColor))) {
            sampleColor = vec4(0.0);
            pct = 0.0;
        }

        color += mix(color/tot, sampleColor, pct);
        tot += 1.0;
        radius += 0.5 / radius;
    }

    FragColor = color / tot;
}
