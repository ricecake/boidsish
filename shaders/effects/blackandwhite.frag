#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;

void main() {
    vec4 color = texture(sceneTexture, TexCoords);
    float average = (color.r + color.g + color.b) / 3.0;
    FragColor = vec4(average, average, average, 1.0);
}
