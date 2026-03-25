#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;

uniform vec3 sunDir; // Direction TO sun
uniform mat4 view;
uniform mat4 projection;

uniform int   samples = 32;
uniform float density = 0.96;
uniform float weight = 0.58;
uniform float decay = 0.9;
uniform float exposure = 0.2;

void main() {
    // 1. Calculate sun screen space position
    vec4 sunClipPos = projection * view * vec4(sunDir * 1000.0, 1.0);
    vec3 sunPosNDC = sunClipPos.xyz / sunClipPos.w;
    vec2 sunScreenPos = sunPosNDC.xy * 0.5 + 0.5;

    // 2. Initial color from scene
    // We want to extract only the bright parts or parts that represent the "sun"
    // For now, let's just use the scene color, but in a more advanced version
    // we would use a mask of the sun disk + bright sky.
    vec3 color = texture(sceneTexture, TexCoords).rgb;

    // Check if sun is behind the camera
    if (sunClipPos.w < 0.0) {
        FragColor = vec4(color, 1.0);
        return;
    }

    vec2 deltaTexCoord = (TexCoords - sunScreenPos);
    deltaTexCoord *= 1.0 / float(samples) * density;

    vec2 uv = TexCoords;
    float illuminationDecay = 1.0;

    vec3 godRays = vec3(0.0);

    for (int i = 0; i < samples; i++) {
        uv -= deltaTexCoord;

        // Sample scene color
        vec3 sampleColor = texture(sceneTexture, uv).rgb;

        // Sample depth to check for occlusion
        float depth = texture(depthTexture, uv).r;

        // If depth < 1.0, it's occluded by geometry (assuming sky is at 1.0)
        // We only want rays from the sky/sun.
        if (depth < 1.0) {
            sampleColor *= 0.0;
        }

        sampleColor *= illuminationDecay * weight;
        godRays += sampleColor;
        illuminationDecay *= decay;
    }

    FragColor = vec4(color + godRays * exposure, 1.0);
}
