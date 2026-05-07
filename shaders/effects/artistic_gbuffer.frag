#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D velocityTexture;
uniform sampler2D normalTexture;
uniform sampler2D albedoTexture;
uniform float     time;

void main() {
    // 1. Sample all textures
    vec4 scene = texture(sceneTexture, TexCoords);
    float depth = texture(depthTexture, TexCoords).r;
    vec2 velocity = texture(velocityTexture, TexCoords).rg;
    vec3 normal = texture(normalTexture, TexCoords).rgb;
    vec4 albedo = texture(albedoTexture, TexCoords);

    // 2. Inappropriate combinations

    // Use velocity to perturb UVs for a motion-driven "painting" effect
    vec2 perturbedUV = TexCoords + velocity * sin(time) * 5.0;
    vec4 perturbedScene = texture(sceneTexture, perturbedUV);

    // Use normal to shift colors
    vec3 colorShift = normal * 0.5 + 0.5;

    // Scale depth to create a "distance-based insanity"
    float depthEffect = pow(depth, 0.5);

    // Combine everything in a weird way
    vec3 finalColor = mix(perturbedScene.rgb, albedo.rgb, depthEffect);
    finalColor *= colorShift;

    // Add some "velocity-based glow"
    finalColor += vec3(length(velocity) * 10.0, 0.0, length(velocity) * 5.0);

    // Edge-case handling for sky (where depth is 1.0)
    if (depth > 0.9999) {
        finalColor = scene.rgb + vec3(sin(time + TexCoords.x * 10.0) * 0.1);
    }

    FragColor = vec4(finalColor, 1.0);
}
