#version 460 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uPulseTexture;
uniform float uBrightness;

void main() {
    vec3 sceneColor = texture(uSceneTexture, TexCoords).rgb;
    float pulseMask = texture(uPulseTexture, TexCoords).r;

    // Dim the scene slightly where no pulse is
    vec3 dimmedScene = sceneColor * 0.1;

    // Add full scene color based on pulse
    vec3 finalColor = mix(dimmedScene, sceneColor * uBrightness, clamp(pulseMask, 0.0, 1.0));

    FragColor = vec4(finalColor, 1.0);
}
