#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform float time;

void main() {
    vec4 color = texture(sceneTexture, TexCoords);
    float shimmer = (sin(time * 10.0 + TexCoords.y * 60.0) + 1.0) / 2.0;
    shimmer = 0.8 + shimmer * 0.2; // subtle effect
    FragColor = vec4(color.rgb * shimmer, 1.0);
}
