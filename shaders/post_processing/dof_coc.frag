#version 460 core

uniform sampler2D depthTexture;
uniform vec2      RESOLUTION;

uniform bool      uAutofocus;
uniform float     uManualFocusDistance;
uniform vec2      uFocalPointOffset;
uniform float     uMinFocusDistance;
uniform float     uMaxFocusDistance;

uniform float     uFocusScale;
uniform mat4      uInvProjection;

in vec2 TexCoords;
out float FragCoC;

float getLinearDepth(vec2 uv) {
    float depth = texture(depthTexture, uv).r;
    if (depth > 0.99999) return 50000.0;

    vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpacePos = uInvProjection * clipSpacePos;
    return -viewSpacePos.z / viewSpacePos.w;
}

void main() {
    float actualFocusDist = uManualFocusDistance;
    if (uAutofocus) {
        vec2 focusUV = vec2(0.5) + uFocalPointOffset;
        actualFocusDist = getLinearDepth(focusUV);
        actualFocusDist = clamp(actualFocusDist, uMinFocusDistance, uMaxFocusDistance);
    }

    float currentDepth = getLinearDepth(TexCoords);

    // CoC calculation using linear depth
    float coc = (1.0/actualFocusDist - 1.0/currentDepth) * uFocusScale;
    FragCoC = clamp(coc, -1.0, 1.0);
}
