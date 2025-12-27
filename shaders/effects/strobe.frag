#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D strobeTexture;

uniform float time;
uniform float lastCaptureTime;
uniform float fadeDuration;

void main()
{
    vec4 sceneColor = texture(sceneTexture, TexCoords);
    vec4 strobeColor = texture(strobeTexture, TexCoords);

    float timeSinceCapture = time - lastCaptureTime;
    float strobeAlpha = 1.0 - smoothstep(0.0, fadeDuration, timeSinceCapture);

    // Only blend if the strobe texture has some content (not black/transparent)
    if (strobeColor.a > 0.01) {
        FragColor = mix(sceneColor, strobeColor, strobeAlpha);
    } else {
        FragColor = sceneColor;
    }
}
