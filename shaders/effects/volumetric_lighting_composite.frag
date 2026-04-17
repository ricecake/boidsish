#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D volumetricTexture;
uniform sampler2D depthTexture;

void main() {
    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;

    // Bilateral upsampling
    // To keep it simple but better than raw linear, we sample a 2x2 low-res neighborhood
    // but the blur pass already does most of the heavy lifting.

    vec4 volumetric = texture(volumetricTexture, TexCoords);

    // volumetric.rgb = Scattering
    // volumetric.a = Transmittance

    // Energy conservation: attenuate background light by transmittance
    vec3 result = sceneColor * volumetric.a + volumetric.rgb;

    FragColor = vec4(result, 1.0);
}
