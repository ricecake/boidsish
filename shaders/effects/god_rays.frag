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

    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;

    // Check if sun is behind the camera or off-screen
    if (sunClipPos.w < 0.0) {
        FragColor = vec4(sceneColor, 1.0);
        return;
    }

    vec2 deltaTexCoord = (TexCoords - sunScreenPos);
    deltaTexCoord *= 1.0 / float(samples) * density;

    vec2 uv = TexCoords;
    float illuminationDecay = 1.0;

    vec3 godRays = vec3(0.0);

    for (int i = 0; i < samples; i++) {
        uv -= deltaTexCoord;

        // Sample depth to check for occlusion BEFORE color
        float depth = texture(depthTexture, uv).r;

        // Only consider pixels with background depth (1.0) as potential sources
        // Using a small epsilon to catch potential floating point precision issues
        if (depth > 0.9999) {
            vec3 sampleColor = texture(sceneTexture, uv).rgb;
            float brightness = dot(sampleColor, vec3(0.2126, 0.7152, 0.0722));

            // Mask for very bright areas (the sun/bright sky)
            float intensity = smoothstep(0.8, 1.0, brightness);
            godRays += sampleColor * intensity * illuminationDecay * weight;
        }

        illuminationDecay *= decay;
    }

    // Apply distance-based falloff from the sun point
    float distFromSun = length(TexCoords - sunScreenPos);
    float falloff = smoothstep(1.0, 0.0, distFromSun);

    FragColor = vec4(sceneColor + godRays * exposure * falloff, 1.0);
}
