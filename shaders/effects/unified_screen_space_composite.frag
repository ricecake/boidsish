#version 430 core
#include "../textures/post_processing.glsl"
out vec4 FragColor;

in vec2 TexCoords;


uniform bool  uSSGIEnabled = true;
uniform bool  uGTAOEnabled = true;
uniform bool  uSSSEnabled = true;

uniform float uSSGIIntensity = 1.0;
uniform float uGTAOIntensity = 1.0;
uniform float uSSSIntensity = 0.5;

// Bilateral upsampling helpers
vec4 sampleBilateral(sampler2D lowResTex, sampler2D highResDepth, vec2 uv) {
    ivec2 lowResSize = textureSize(lowResTex, 0);
    vec2 lowResInvSize = 1.0 / vec2(lowResSize);

    // Nearest low-res texel center
    vec2 lowResUV = uv * vec2(lowResSize);
    ivec2 baseCoord = ivec2(floor(lowResUV - 0.5));
    vec2 f = fract(lowResUV - 0.5);

    float highDepth = texture(highResDepth, uv).r;

    vec4 sumColor = vec4(0.0);
    float sumWeight = 0.0;

    for (int y = 0; y <= 1; y++) {
        for (int x = 0; x <= 1; x++) {
            ivec2 coord = baseCoord + ivec2(x, y);
            vec2 sampleUV = (vec2(coord) + 0.5) * lowResInvSize;

            vec4 color = textureLod(lowResTex, sampleUV, 0.0);
            float lowDepth = textureLod(highResDepth, sampleUV, 0.0).r; // Use high-res depth at low-res locations for better matching

            float spatialW = (x == 0 ? 1.0 - f.x : f.x) * (y == 0 ? 1.0 - f.y : f.y);
            float depthW = 1.0 / (0.0001 + abs(highDepth - lowDepth) * 1000.0); // Sharp depth weight

            float weight = spatialW * depthW;
            sumColor += color * weight;
            sumWeight += weight;
        }
    }

    return sumColor / max(sumWeight, 0.0001);
}

void main() {
	vec4 color = texture(u_sceneTexture, TexCoords);

    // Use bilateral upsampling for low-res effects
	vec4 giao = sampleBilateral(u_giaoTexture, u_depthTexture, TexCoords);
	float sssFactor = sampleBilateral(u_sssTexture, u_depthTexture, TexCoords).r;

	float traditionalShadow = texture(u_normalTexture, TexCoords).a;

	vec3 result = color.rgb;

	// 1. Apply Screen Space Shadows
	if (uSSSEnabled) {
		float relativeSSS = clamp(sssFactor / max(traditionalShadow, 0.001), 0.0, 1.0);
		float shadowFactor = mix(1.0, relativeSSS, uSSSIntensity);
		result *= shadowFactor;
	}

	// 2. Apply Ambient Occlusion (GTAO)
	if (uGTAOEnabled) {
		float ao = giao.a;
		result *= ao;
	}

	// 3. Apply Global Illumination (SSGI)
	if (uSSGIEnabled) {
		vec3 ssgi = giao.rgb;
		result += ssgi;
	}

	FragColor = vec4(result, color.a);
}
