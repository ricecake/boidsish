#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uSssrTexture;

void main() {
    vec4 scene = texture(uSceneTexture, TexCoords);
    vec4 sssr = texture(uSssrTexture, TexCoords);

    // Additive blending for reflections.
    // Since sssr.rgb already includes intensity, fresnel (weight), and roughness fade,
    // we simply add it to the scene. sssr.a is used as a hit-mask but is also weighted.
    FragColor = vec4(scene.rgb + sssr.rgb, scene.a);
}
