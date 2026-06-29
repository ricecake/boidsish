#version 460 core

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform vec2      RESOLUTION;

uniform bool      uAutofocus;
uniform float     uManualFocusDistance;
uniform vec2      uFocalPointOffset;
uniform float     uFocusScale;
uniform float     uBlurSize;
uniform mat4      uInvProjection;

in vec2 TexCoords;
out vec4 FragColor;

// Robust linear depth reconstruction
float getLinearDepth(vec2 uv) {
    float d = textureLod(depthTexture, uv, 0.0).r;
    if (d >= 0.999) return 100000.0;

    vec4 clip = vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec4 view = uInvProjection * clip;

    if (abs(view.w) < 0.00001) return 100000.0;
    float linearZ = -view.z / view.w;

    if (isnan(linearZ) || isinf(linearZ)) return 100000.0;
    return clamp(linearZ, 0.1, 100000.0);
}

vec3 safe_color(vec3 c) {
    if (any(isnan(c)) || any(isinf(c))) return vec3(0.0);
    return max(c, vec3(0.0));
}

void main() {
    vec4  centerColorRaw = textureLod(screenTexture, TexCoords, 0.0);
    float centerDepth = textureLod(depthTexture, TexCoords, 0.0).r;

    // 1. ABSOLUTE SKY PROTECTION: If we're looking at the sky, don't blur.
    if (centerDepth >= 0.99) {
        FragColor = vec4(safe_color(centerColorRaw.rgb), centerColorRaw.a);
        return;
    }

    // 2. Determine focal distance
    float focusDist = uManualFocusDistance;
    if (uAutofocus) {
        focusDist = getLinearDepth(vec2(0.5) + uFocalPointOffset);
    }
    focusDist = clamp(focusDist, 0.1, 100000.0);

    // 3. Calculate local CoC
    float centerLinearDepth = getLinearDepth(TexCoords);
    float coc = clamp((1.0/focusDist - 1.0/centerLinearDepth) * uFocusScale, -1.0, 1.0);
    float blurRadius = abs(coc) * uBlurSize;

    // 4. Early exit for sharp regions
    if (blurRadius < 0.1) {
        FragColor = vec4(safe_color(centerColorRaw.rgb), centerColorRaw.a);
        return;
    }

    // 5. Weighted Bokeh Accumulation
    vec2  pixelSize = 1.0 / RESOLUTION;
    vec3  colorSum = vec3(0.0);
    float totalWeight = 0.0;

    // Center sample
    vec3 centerRGB = safe_color(centerColorRaw.rgb);
    colorSum += centerRGB;
    totalWeight += 1.0;

    const int SAMPLES = 32;
    const float GOLDEN_ANGLE = 2.39996323;

    for (int i = 0; i < SAMPLES; i++) {
        float r = sqrt(float(i) + 0.5) / sqrt(float(SAMPLES));
        float theta = float(i) * GOLDEN_ANGLE;

        float currentSampleRadius = r * blurRadius;
        vec2 tc = TexCoords + vec2(cos(theta), sin(theta)) * currentSampleRadius * pixelSize;

        float sampleDepth = textureLod(depthTexture, tc, 0.0).r;
        vec3  sampleRGB = safe_color(textureLod(screenTexture, tc, 0.0).rgb);

        float weight = 1.0;

        // Background bleeding prevention
        if (sampleDepth > centerDepth + 0.0001) {
            float sampleLinearDepth = getLinearDepth(tc);
            float sampleCoc = clamp((1.0/focusDist - 1.0/sampleLinearDepth) * uFocusScale, -1.0, 1.0);
            weight = smoothstep(currentSampleRadius + 0.5, currentSampleRadius - 0.5, abs(sampleCoc) * uBlurSize);
        }

        // Sky contribution
        if (sampleDepth >= 0.99) weight = 1.0;

        colorSum += sampleRGB * weight;
        totalWeight += weight;
    }

    FragColor = vec4(colorSum / totalWeight, centerColorRaw.a);
}
