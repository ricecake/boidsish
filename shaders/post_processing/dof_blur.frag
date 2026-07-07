#version 460 core

uniform sampler2D screenTexture;
uniform sampler2D cocTexture;
uniform sampler2D depthTexture;
uniform vec2      RESOLUTION;

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

void main() {
    float coc = texture(cocTexture, TexCoords).r;
    float currentDepth = getLinearDepth(TexCoords);
    float blurSize = abs(coc) * uBlurSize;

    vec2 pixelSize = 1.0 / RESOLUTION;
    vec3 color = texture(screenTexture, TexCoords).rgb;

    vec3 colorSum = color;
    float totalWeight = 1.0;

    const int SAMPLES = 32;
    const float GOLDEN_ANGLE = 2.39996323;

    for (int i = 0; i < SAMPLES; i++) {
        float r = sqrt(float(i) + 0.5) / sqrt(float(SAMPLES));
        float theta = float(i) * GOLDEN_ANGLE;

        float currentSampleRadius = r * blurSize;
        vec2 tc = TexCoords + vec2(cos(theta), sin(theta)) * currentSampleRadius * pixelSize;

        float sampleDepth = getLinearDepth(tc);
        float sampleCoc = texture(cocTexture, tc).r;
        float sampleBlurRadius = abs(sampleCoc) * uBlurSize;

        float weight = 1.0;
        if (sampleDepth > currentDepth) {
            weight = smoothstep(currentSampleRadius + 0.5, currentSampleRadius - 0.5, sampleBlurRadius);
        }
        vec3 sampleColor = texture(screenTexture, tc).rgb;
        colorSum += sampleColor * weight;
        totalWeight += weight;
    }

    FragColor = vec4(colorSum / totalWeight, 1.0);
}
