#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D cloudTexture;
uniform sampler2D depthTexture;
uniform sampler2D lowResDepthTexture;

void main() {
    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
    vec4 cloudColor = texture(cloudTexture, TexCoords); // Simple upsample for now

    // Bilateral upsample logic could go here

    vec3 finalColor = sceneColor * (1.0 - cloudColor.a) + cloudColor.rgb;
    FragColor = vec4(finalColor, 1.0);
}
