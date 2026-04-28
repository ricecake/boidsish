#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uGIAOTexture;  // RGB: GI, A: AO
uniform sampler2D uSSSTexture;   // R: SSS
uniform sampler2D uNormalTexture; // A: traditional shadow
uniform sampler2D uDepthTexture;  // High-res depth
uniform sampler2D uDITexture;     // RGB: ReSTIR DI

uniform bool  uSSGIEnabled = true;
uniform bool  uRestirDIEnabled = true;
uniform bool  uRestirGIEnabled = true;
uniform bool  uGTAOEnabled = true;
uniform bool  uSSSEnabled = true;

uniform float uSSGIIntensity = 1.0;
uniform float uRestirDIIntensity = 1.0;
uniform float uRestirGIIntensity = 1.0;
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


vec4 sampleBilateral(sampler2D lowResTex, sampler2D highResDepth, sampler2D highResNormal, vec2 uv) {
	ivec2 lowResSize = textureSize(lowResTex, 0);
	vec2 lowResInvSize = 1.0 / vec2(lowResSize);

	// Get the high-res targets
	float centerDepth = texture(highResDepth, uv).r;
	vec3 centerNormal = normalize(texture(highResNormal, uv).xyz);

	vec2 lowResUV = uv * vec2(lowResSize);
	vec2 baseCoord = floor(lowResUV - 0.5);
	vec2 f = fract(lowResUV - 0.5);

	vec4 sumColor = vec4(0.0);
	float sumWeight = 0.0;

	// Use a slightly softer depth tolerance, adjust based on your projection matrix
	const float depthSigma = 0.05;
	const float normalSigma = 32.0; // Higher means tighter normal threshold

	// 2x2 footprint is the minimum, but a 3x3 gathers more neighbor data for smoothing
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 offsetUV = (baseCoord + vec2(x, y) + 0.5) * lowResInvSize;

			vec4 color = texture(lowResTex, offsetUV);
			float sampleDepth = texture(highResDepth, offsetUV).r;

			vec3 rawNormal = texture(highResNormal, offsetUV).xyz;
			// Only normalize if the vector has length, otherwise default to a safe vector
			vec3 sampleNormal = dot(rawNormal, rawNormal) > 0.001 ? normalize(rawNormal) : vec3(0.0, 1.0, 0.0);

			// 1. Spatial Weight (Gaussian-ish distance from center)
			float spatialW = exp(-float(x*x + y*y) / 2.0);

			// 2. Depth Weight
			float depthDiff = abs(centerDepth - sampleDepth);
			float depthW = exp(-(depthDiff * depthDiff) / (2.0 * depthSigma * depthSigma));
			// float depthW = 1.0 / (0.0001 + abs(centerDepth - sampleDepth) * 1000.0); // Sharp depth weight


			// 3. Normal Weight
			float normalW = pow(max(dot(centerNormal, sampleNormal), 0.0), normalSigma);

			float weight = spatialW * depthW * normalW;

			sumColor += color * weight;
			sumWeight += weight;
		}
	}

	return sumColor / max(sumWeight, 0.0001);
}

void main() {
	float centerDepth = texture(uDepthTexture, TexCoords).r;

	// Early-out for the sky/atmosphere
	// If you are using a reversed-Z depth buffer, this should be <= 0.0
	vec4 color = texture(uSceneTexture, TexCoords);
	if (centerDepth >= 1.0) {
		FragColor = color;
		return;
	}

	// Use bilateral upsampling for low-res effects
	vec4 giao = sampleBilateral(uGIAOTexture, uDepthTexture, uNormalTexture, TexCoords);
	float sssFactor = sampleBilateral(uSSSTexture, uDepthTexture, uNormalTexture, TexCoords).r;
	vec3 di_lighting = sampleBilateral(uDITexture, uDepthTexture, uNormalTexture, TexCoords).rgb;

	// Basic firefly rejection: clamp ReSTIR contribution based on scene luminance
	float max_lum = max(color.r, max(color.g, color.b)) * 10.0 + 2.0;
	giao.rgb = min(giao.rgb, vec3(max_lum));
	di_lighting = min(di_lighting, vec3(max_lum));

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

	// 3. Apply ReSTIR Direct Illumination
	if (uRestirDIEnabled) {
		result += di_lighting; // Intensity already applied in compute
	}

	// 4. Apply Global Illumination (SSGI)
	if (uSSGIEnabled) {
		vec3 ssgi = giao.rgb;
		result += ssgi * uSSGIIntensity;
	}

	FragColor = vec4(result, color.a);
}
