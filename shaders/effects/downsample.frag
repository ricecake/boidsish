#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sourceTexture;
uniform int       mipLevel;

void main() {
    vec2 texelSize = 1.0 / textureSize(sourceTexture, 0);
    vec3 result = vec3(0.0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            result += texture(sourceTexture, TexCoords + vec2(x, y) * texelSize).rgb;
        }
    }
    float luminance = dot(result / 9.0, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(luminance, 0.0, 0.0, 1.0);
}
