#version 420 core
out vec2 FragColor;

in vec2 TexCoords;

uniform sampler2D depthTexture;
uniform mat4 invViewProj;
uniform mat4 prevViewProj;

void main() {
    float depth = texture(depthTexture, TexCoords).r;

    // Reconstruct world position
    vec4 currentPos = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = invViewProj * currentPos;
    worldPos /= worldPos.w;

    // Project into previous frame
    vec4 prevPos = prevViewProj * worldPos;
    prevPos /= prevPos.w;

    // Convert to [0,1] UV space
    vec2 prevTexCoords = prevPos.xy * 0.5 + 0.5;

    // Motion vector is difference in UVs
    FragColor = TexCoords - prevTexCoords;
}
