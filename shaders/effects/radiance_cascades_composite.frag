#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2DArray uCascadesTexture;
uniform float uIntensity;

void main() {
    vec3 sceneColor = texture(uSceneTexture, TexCoords).rgb;

    ivec2 pixelPos = ivec2(gl_FragCoord.xy);
    ivec2 baseTexel = pixelPos * 2;

    vec3 indirectLight = vec3(0.0);
    indirectLight += texelFetch(uCascadesTexture, ivec3(baseTexel, 0), 0).rgb;
    indirectLight += texelFetch(uCascadesTexture, ivec3(baseTexel + ivec2(1, 0), 0), 0).rgb;
    indirectLight += texelFetch(uCascadesTexture, ivec3(baseTexel + ivec2(0, 1), 0), 0).rgb;
    indirectLight += texelFetch(uCascadesTexture, ivec3(baseTexel + ivec2(1, 1), 0), 0).rgb;
    indirectLight /= 4.0;

    vec3 result = sceneColor + indirectLight * uIntensity;

    FragColor = vec4(result, 1.0);
}
