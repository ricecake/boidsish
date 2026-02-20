#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uSssrTexture;

void main() {
    vec4 scene = texture(uSceneTexture, TexCoords);
    vec4 sssr = texture(uSssrTexture, TexCoords);

    // Additive blending for reflections
    // In a full PBR pipeline, this would be more complex (fresnel, etc.)
    // But since sssr already contains the radiance, we can just add it
    // or mix it.

    FragColor = vec4(scene.rgb + sssr.rgb * sssr.a, scene.a);
}
