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

    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            vec2 uv = TexCoords + vec2(x, y) * texelSize;
            float sampleDepth = texture(depthTexture, uv).r;

            // Gaussian weight based on space
            float spaceWeight = exp(-0.5 * (x*x + y*y) / (1.0 * 1.0));

            // Exponential weight based on depth similarity
            float depthWeight = exp(-abs(depth - sampleDepth) * 5000.0);

            float weight = spaceWeight * depthWeight;

            cloudColor += texture(cloudTexture, uv) * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight > 1e-6) {
        cloudColor /= totalWeight;
    } else {
        cloudColor = texture(cloudTexture, TexCoords);
    }

    // Safety check for NaN
    if (any(isnan(cloudColor))) cloudColor = vec4(0.0);

    vec3 finalColor = sceneColor * (1.0 - cloudColor.a) + cloudColor.rgb;
    FragColor = vec4(finalColor, 1.0);
}
