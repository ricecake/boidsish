#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D cloudTexture;
uniform sampler2D depthTexture;

void main() {
    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;

    // Bilateral Upsample
    vec2 texelSize = 1.0 / textureSize(cloudTexture, 0);
    float depth = texture(depthTexture, TexCoords).r;

    vec4 cloudColor = vec4(0.0);
    float totalWeight = 0.0;

    // 3x3 kernel for upsampling
    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            vec2 uv = TexCoords + vec2(x, y) * texelSize;
            float sampleDepth = texture(depthTexture, uv).r;

            // Weight based on depth similarity
            float weight = 1.0 / (0.0001 + abs(depth - sampleDepth) * 1000.0);

            // Further weight by distance from center
            weight *= (1.0 - length(vec2(x, y)) * 0.25);

            cloudColor += texture(cloudTexture, uv) * weight;
            totalWeight += weight;
        }
    }

    cloudColor /= max(totalWeight, 0.0001);

    // Composite clouds over scene
    vec3 finalColor = sceneColor * (1.0 - cloudColor.a) + cloudColor.rgb;
    FragColor = vec4(finalColor, 1.0);
}
