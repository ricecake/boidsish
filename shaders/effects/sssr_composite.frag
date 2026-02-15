#version 420 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D reflectionTexture;
uniform sampler2D gNormal;
uniform sampler2D gDepth;
uniform mat4      uInvProjection;

vec3 getPosition(vec2 uv) {
    float depth = texture(gDepth, uv).r;
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = uInvProjection * clip;
    return view.xyz / view.w;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 scene = texture(sceneTexture, TexCoords);
    vec4 reflection = texture(reflectionTexture, TexCoords);

    vec3 normal = texture(gNormal, TexCoords).xyz * 2.0 - 1.0;
    if (length(normal) < 0.1) {
        FragColor = scene;
        return;
    }

    vec3 posView = getPosition(TexCoords);
    vec3 viewDir = normalize(-posView);

    // Assuming a standard F0 for dielectrics if we don't have metallic/albedo here
    vec3 F0 = vec3(0.04);
    vec3 F = fresnelSchlick(max(dot(normal, viewDir), 0.0), F0);

    // Mix based on Fresnel - ensure energy conservation roughly
    vec3 finalColor = scene.rgb * (1.0 - F) + reflection.rgb * F;

    FragColor = vec4(finalColor, scene.a);
}
