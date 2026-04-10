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

	// The scene color already has traditional shadow applied.
	// We only want to apply additional shadowing if SSS is darker than the traditional shadow.
	// relativeSSS is 1.0 if sssFactor >= traditionalShadow, and < 1.0 otherwise.
	float relativeSSS = clamp(sssFactor / max(traditionalShadow, 0.001), 0.0, 1.0);

	// Apply intensity only to the delta provided by SSS
	float shadowFactor = mix(1.0, relativeSSS, uIntensity);

	FragColor = vec4(sceneColor.rgb * shadowFactor, sceneColor.a);
}
