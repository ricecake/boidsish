#version 460 core

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform vec2      RESOLUTION;
uniform float     uFocusPoint;
uniform float     uFocusScale;
uniform float     uBlurSize;

uniform bool      uAutofocus;
uniform vec2      uFocusOffset;
uniform float     uMinFocusDistance;
uniform float     uMaxFocusDistance;
uniform mat4      uInvProjection;

#define SAMPLEDOF_BLUR_SIZE uBlurSize

// Helper to get linear depth from hardware depth
float getLinearDepth(vec2 uv) {
    float depth = texture(depthTexture, uv).r;
    vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpacePos = uInvProjection * clipSpacePos;
    return -viewSpacePos.z / viewSpacePos.w;
}

// Custom sampleDoF that handles autofocus and linear depth
vec3 sampleDoF_enhanced(sampler2D tex, sampler2D texDepth, vec2 st, float focusPoint, float focusScale) {
    float actualFocusDist = focusPoint;
    if (uAutofocus) {
        vec2 focusUV = vec2(0.5) + uFocusOffset;
        actualFocusDist = getLinearDepth(focusUV);
        actualFocusDist = clamp(actualFocusDist, uMinFocusDistance, uMaxFocusDistance);
    }

    float currentDepth = getLinearDepth(st);

    // CoC calculation using linear depth
    float coc = clamp((1.0/actualFocusDist - 1.0/currentDepth) * focusScale, -1.0, 1.0);
    float blurSize = abs(coc) * SAMPLEDOF_BLUR_SIZE;

    vec2 pixelSize = 1.0 / RESOLUTION;
    vec3 color = texture(tex, st).rgb;

    float tot = 1.0;
    float radius = 0.5; // SAMPLEDOF_RAD_SCALE
    const float GOLDEN_ANGLE = 2.39996323;

    for (float ang = 0.0; radius < blurSize; ang += GOLDEN_ANGLE) {
        vec2 tc = st + vec2(cos(ang), sin(ang)) * pixelSize * radius;
        float sampleDepth = getLinearDepth(tc);

        float sampleCoc = clamp((1.0/actualFocusDist - 1.0/sampleDepth) * focusScale, -1.0, 1.0);
        float sampleBlurSize = abs(sampleCoc) * SAMPLEDOF_BLUR_SIZE;

        if (sampleDepth > currentDepth)
            sampleBlurSize = clamp(sampleBlurSize, 0.0, blurSize * 2.0);

        float pct = smoothstep(radius - 0.5, radius + 0.5, sampleBlurSize);
        vec3 sampleColor = texture(tex, tc).rgb;

        color += mix(color/tot, sampleColor, pct);
        tot += 1.0;
        radius += 0.5 / radius;
    }

    return color / tot;
}

in vec2 TexCoords;
out vec4 FragColor;

void main() {
    FragColor = vec4(sampleDoF_enhanced(screenTexture, depthTexture, TexCoords, uFocusPoint, uFocusScale), 1.0);
}
