#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uGIAOTexture;  // RGB: GI, A: AO
uniform sampler2D uSSSTexture;   // R: SSS
uniform sampler2D uNormalTexture; // A: traditional shadow

uniform bool  uSSGIEnabled = true;
uniform bool  uGTAOEnabled = true;
uniform bool  uSSSEnabled = true;

uniform float uSSGIIntensity = 1.0;
uniform float uGTAOIntensity = 1.0;
uniform float uSSSIntensity = 0.5;

void main() {
	vec4 color = texture(uSceneTexture, TexCoords);
	vec4 giao = texture(uGIAOTexture, TexCoords);
	float sssFactor = texture(uSSSTexture, TexCoords).r;
	float traditionalShadow = texture(uNormalTexture, TexCoords).a;

	vec3 result = color.rgb;

	// 1. Apply Screen Space Shadows
	if (uSSSEnabled) {
		float relativeSSS = clamp(sssFactor / max(traditionalShadow, 0.001), 0.0, 1.0);
		float shadowFactor = mix(1.0, relativeSSS, uSSSIntensity);
		result *= shadowFactor;
	}

	// 2. Apply Ambient Occlusion (GTAO)
	// Intensity was already applied in the compute shader to AO
	if (uGTAOEnabled) {
		float ao = giao.a;
		result *= ao;
	}

	// 3. Apply Global Illumination (SSGI)
	// Intensity was already applied in the compute shader to SSGI
	if (uSSGIEnabled) {
		vec3 ssgi = giao.rgb;
		result += ssgi;
	}

	FragColor = vec4(result, color.a);
}
