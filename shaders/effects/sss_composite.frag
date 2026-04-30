#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uShadowMask;
uniform sampler2D uNormalTexture;
uniform float     uIntensity = 0.5;

void main() {
	vec4  sceneColor = texture(uSceneTexture, TexCoords);
	float sssFactor = texture(uShadowMask, TexCoords).r;
	float traditionalShadow = texture(uNormalTexture, TexCoords).a;

	// Improved logic to avoid double-shadowing:
	// The scene color already has traditional shadow applied.
	// We want the final shadow to be the minimum of both systems.
	// finalShadow = min(traditionalShadow, sssFactor)
	// Since color is already (baseColor * traditionalShadow),
	// we multiply by (finalShadow / traditionalShadow).

	float combinedShadow = min(traditionalShadow, sssFactor);

	// Use a small epsilon to avoid division by zero and handle fully shadowed areas
	float shadowAdjustment = combinedShadow / max(traditionalShadow, 0.01);

	// Apply intensity mix
	float shadowFactor = mix(1.0, shadowAdjustment, uIntensity);

	FragColor = vec4(sceneColor.rgb * shadowFactor, sceneColor.a);
}
