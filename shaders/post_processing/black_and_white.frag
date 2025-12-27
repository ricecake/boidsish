#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;

void main() {
    vec4 color = texture(sceneTexture, TexCoords);
    float average = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
    FragColor = vec4(average, average, average, 1.0);
}
