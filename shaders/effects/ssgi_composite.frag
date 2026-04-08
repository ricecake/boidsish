#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D ssgiTexture;
uniform float     uIntensity = 1.0;

void main() {
	vec4 color = texture(sceneTexture, TexCoords);
	vec4 ssgi = texture(ssgiTexture, TexCoords);

	// SSGI typically adds to the lit scene
	// We use uIntensity to control the contribution
	vec3 result = color.rgb + ssgi.rgb * uIntensity;

	FragColor = vec4(result, color.a);
}
