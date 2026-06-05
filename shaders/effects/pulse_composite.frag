#version 460 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uPulseTexture;
uniform float uBrightness;
uniform float uAmbientBrightness;

void main() {
    vec3 sceneColor = texture(uSceneTexture, TexCoords).rgb;
    float pulseMask = texture(uPulseTexture, TexCoords).r;

    // Dim the scene based on ambient parameter
    vec3 dimmedScene = sceneColor * uAmbientBrightness;

    // Add scene color boosted by pulse
    // Use addition for "glow" feel or mix for "illumination" feel
    vec3 illuminated = sceneColor * uBrightness;

    // Smooth the mask a bit
    float pulseFactor = clamp(pulseMask, 0.0, 1.0);

    vec3 finalColor = mix(dimmedScene, illuminated, pulseFactor);

    // Add a bit of extra glow at the highlights
    finalColor += vec3(0.5, 0.7, 1.0) * pulseFactor * 0.2;

    FragColor = vec4(finalColor, 1.0);
}
