#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uGIAOTexture;  // RGB: GI, A: AO
uniform sampler2D uSSSTexture;   // R: SSS
uniform sampler2D uNormalTexture; // A: traditional shadow
uniform sampler2D uDepthTexture;  // High-res depth

uniform bool  uSSGIEnabled = true;
uniform bool  uGTAOEnabled = true;
uniform bool  uSSSEnabled = true;

uniform float uSSGIIntensity = 1.0;
uniform float uGTAOIntensity = 1.0;
uniform float uSSSIntensity = 0.5;

// Bilateral upsampling helpers
vec4 sampleBilateral(sampler2D lowResTex, sampler2D highResDepth, vec2 uv) {
    ivec2 lowResSize = textureSize(lowResTex, 0);
    if (lowResSize.x == 0 || lowResSize.y == 0) return vec4(0.0);

    vec2 lowResInvSize = 1.0 / vec2(lowResSize);

    // Nearest low-res texel center
    vec2 lowResUV = uv * vec2(lowResSize);
    ivec2 baseCoord = ivec2(floor(lowResUV - 0.5));
    vec2 f = fract(lowResUV - 0.5);

    float highDepth = textureLod(highResDepth, uv, 0.0).r;

    vec4 sumColor = vec4(0.0);
    float sumWeight = 0.0;

    for (int y = 0; y <= 1; y++) {
        for (int x = 0; x <= 1; x++) {
            ivec2 coord = baseCoord + ivec2(x, y);
            // Clamp coord to valid low-res texture range
            coord = clamp(coord, ivec2(0), lowResSize - ivec2(1));
            vec2 sampleUV = (vec2(coord) + 0.5) * lowResInvSize;

            vec4 color = textureLod(lowResTex, sampleUV, 0.0);
            // Sample depth at the center of the low-res texel
            float lowDepth = textureLod(highResDepth, sampleUV, 0.0).r;

            float spatialW = (x == 0 ? 1.0 - f.x : f.x) * (y == 0 ? 1.0 - f.y : f.y);
            // High-precision depth weight
            float depthW = 1.0 / (0.0001 + abs(highDepth - lowDepth) * 2000.0);

            float weight = spatialW * depthW;
            sumColor += color * weight;
            sumWeight += weight;
        }
    }

    // Fallback to bilinear if bilateral weight is too small (e.g. extreme depth discontinuities)
    if (sumWeight < 0.001) {
        return textureLod(lowResTex, uv, 0.0);
    }

    return sumColor / sumWeight;
}

void main() {
	vec4 color = texture(uSceneTexture, TexCoords);

    // Use bilateral upsampling for low-res effects
	vec4 giao = sampleBilateral(uGIAOTexture, uDepthTexture, TexCoords);
	float sssFactor = sampleBilateral(uSSSTexture, uDepthTexture, TexCoords).r;

    // Basic firefly rejection: clamp ReSTIR contribution based on scene luminance
    float max_gi = max(color.r, max(color.g, color.b)) * 10.0 + 1.0;
    giao.rgb = min(giao.rgb, vec3(max_gi));

	float traditionalShadow = texture(uNormalTexture, TexCoords).a;

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

	// 3. Apply Global Illumination (SSGI) and ReSTIR
	if (uSSGIEnabled) {
		// ReSTIR DI provides primary local light.
		vec3 restir_ssgi = giao.rgb;

		// If the original scene has very low intensity (shadows), ReSTIR should dominate.
		float sceneLum = dot(result, vec3(0.2126, 0.7152, 0.0722));
		float restirLum = dot(restir_ssgi, vec3(0.2126, 0.7152, 0.0722));

		// For now, we add it with a more conservative intensity.
        // We use clamp to ensure we don't return NaNs if ReSTIR was corrupt.
        restir_ssgi = clamp(restir_ssgi, vec3(0.0), vec3(100.0));
		result += restir_ssgi * uSSGIIntensity;
	}

	FragColor = vec4(result, clamp(color.a, 0.0, 1.0));
    if (isnan(FragColor.r) || isinf(FragColor.r)) FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
