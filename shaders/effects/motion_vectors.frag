#version 420 core
layout(location = 0) out vec2 FragColor;

in vec2 TexCoords;

uniform sampler2D depthTexture;

uniform mat4 invProjection;
uniform mat4 invView;
uniform mat4 prevView;
uniform mat4 prevProjection;

void main() {
    float depth = texture(depthTexture, TexCoords).r;

    // Convert to clip space
    vec4 clipPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);

    // Convert to world space
    vec4 worldPos = invView * invProjection * clipPos;
    worldPos /= worldPos.w;

    // Convert to previous clip space
    vec4 prevClipPos = prevProjection * prevView * worldPos;
    prevClipPos /= prevClipPos.w;

    // Convert to UV space
    vec2 prevUV = prevClipPos.xy * 0.5 + 0.5;

    // Motion vector is current UV minus previous UV
    FragColor = TexCoords - prevUV;
}
