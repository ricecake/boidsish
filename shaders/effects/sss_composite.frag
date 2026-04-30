#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uShadowMask;
uniform float     uIntensity = 0.5;

void main() {
	vec4  sceneColor = texture(uSceneTexture, TexCoords);
	float shadow = texture(uShadowMask, TexCoords).r;

	// Simple multiplication of the scene color by the shadow factor
	// We mix based on intensity to allow tuning
	float shadowFactor = mix(1.0, shadow, uIntensity);

	FragColor = vec4(sceneColor.rgb * shadowFactor, sceneColor.a);
}
