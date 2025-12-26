#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;

void main() {
    float h = 1.0 / textureSize(sceneTexture, 0).x;
    float v = 1.0 / textureSize(sceneTexture, 0).y;

    float tx0y0 = texture(sceneTexture, TexCoords + vec2(-h, -v)).r;
    float tx1y0 = texture(sceneTexture, TexCoords + vec2(0, -v)).r;
    float tx2y0 = texture(sceneTexture, TexCoords + vec2(h, -v)).r;

    float tx0y1 = texture(sceneTexture, TexCoords + vec2(-h, 0)).r;
    float tx2y1 = texture(sceneTexture, TexCoords + vec2(h, 0)).r;

    float tx0y2 = texture(sceneTexture, TexCoords + vec2(-h, v)).r;
    float tx1y2 = texture(sceneTexture, TexCoords + vec2(0, v)).r;
    float tx2y2 = texture(sceneTexture, TexCoords + vec2(h, v)).r;

    float Gx = (tx0y0 + 2.0 * tx0y1 + tx0y2) - (tx2y0 + 2.0 * tx2y1 + tx2y2);
    float Gy = (tx0y0 + 2.0 * tx1y0 + tx2y0) - (tx0y2 + 2.0 * tx1y2 + tx2y2);

    float magnitude = sqrt(Gx * Gx + Gy * Gy);
    FragColor = vec4(vec3(magnitude), 1.0);
}
