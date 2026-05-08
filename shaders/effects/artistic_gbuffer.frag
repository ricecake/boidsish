#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform sampler2D velocityTexture;
uniform sampler2D normalTexture;
uniform sampler2D albedoTexture;
uniform float     time;

const float near = 1.0;
const float far = 600.0;

float LinearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main() {
    // 1. Sample all textures
    vec4 scene = texture(sceneTexture, TexCoords);
    float depth = texture(depthTexture, TexCoords).r;
    vec2 velocity = texture(velocityTexture, TexCoords).rg;
    vec3 normal = texture(normalTexture, TexCoords).rgb;
    vec4 albedo = texture(albedoTexture, TexCoords);
    float rough = texture(velocityTexture, TexCoords).b;

/*
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
*/
    vec2 perturbedUV = TexCoords + normal.xz;
    vec4 perturbedScene = texture(sceneTexture, perturbedUV);

	vec2  circ = TexCoords - vec2(0.5);
	float distSq = dot(circ, circ);
	// float shapeMask = (1.0-smoothstep(0.20, 0.250, distSq));
	float shapeMask = (1.0-smoothstep(0.01, 0.250, distSq));

    scene = perturbedScene;

    vec4 afterImage = scene - albedo;
    float  average = (albedo.r + albedo.g + albedo.b)/3.0;
    vec4 grey = vec4(average);
    // vec4 grey = scene - afterImage;
    // vec4 finalColor = scene+(afterImage*(LinearizeDepth(pow(depth, 0.5))/far ));
    float depthBucket = floor(20.0*LinearizeDepth(depth)/far)/20.0;
    // vec4 finalColor = scene+(afterImage*( depthBucket ));
    // vec4 finalColor = grey + vec4(step(0.33, depthBucket), step(0.66, depthBucket), step(1.0, depthBucket), 1.0 );
    // vec4 finalColor = grey + (1.0-dot(TexCoords.xy, TexCoords.xy))* vec4(
    //     afterImage.r*smoothstep(0.0, 0.33, depthBucket),
    //     afterImage.g*smoothstep(0.33, 0.66, depthBucket),
    //     afterImage.b*smoothstep(0.66, 1.0, depthBucket),
    //     1.0
    // );

    // vec4 finalColor = grey + (shapeMask) * albedo * smoothstep(0, 0.25, depthBucket) * (1.0-smoothstep(0.35, 0.6, depthBucket));
    // vec4 finalColor = grey + rough * vec4(afterImage) * (shapeMask) * smoothstep(0, 0.25, depthBucket) * (1.0-smoothstep(0.35, 0.6, depthBucket));
    vec4 finalColor = mix(grey, scene, dot(normal, vec3(0.5, 0.5, 0)));


    FragColor = vec4(finalColor.xyz, 1.0);
}
