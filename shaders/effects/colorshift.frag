#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;

void main() {
    vec2 offset = vec2(0.005, 0.005);
    float r = texture(sceneTexture, TexCoords + offset).r;
    float g = texture(sceneTexture, TexCoords).g;
    float b = texture(sceneTexture, TexCoords - offset).b;
    FragColor = vec4(r, g, b, 1.0);
}
