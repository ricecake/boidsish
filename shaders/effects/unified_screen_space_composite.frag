#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uSceneTexture;
uniform sampler2D uGIAOTexture;  // RGB: GI, A: AO
uniform sampler2D uSSSTexture;   // R: SSS
uniform sampler2D uNormalTexture; // A: traditional shadow
uniform sampler2D uDepthTexture;  // High-res depth
uniform sampler2D uDITexture;     // RGB: ReSTIR DI
uniform sampler2D uVelocityTexture;
uniform sampler2D uRawGIAOTexture;
uniform sampler2D uRawDITexture;
uniform sampler2D uHistoryGIAOTexture;
uniform sampler2D uHistoryDITexture;

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
float luminance(vec3 c) {
	return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec4 tentUpsample(sampler2D srcTexture, float filterRadius, float srcLod, vec2 TexCoords) {
	vec2 srcResolution = textureSize(srcTexture, 0);
	filterRadius = max(1.0, filterRadius);
	srcLod = max(0.0, srcLod);

	// Dual Kawase blur upsample with tent filter
	// Provides smooth, high-quality upsampling with natural blur
	vec2  texelSize = 1.0 / srcResolution;
	float radius = filterRadius;

	// 9-tap tent filter for smooth upsampling
	vec3 result = vec3(0.0);

	// Center sample (weight 4)
	result += textureLod(srcTexture, TexCoords, srcLod).rgb * 4.0;

	// Diagonal samples at half-pixel offset (weight 1 each)
	result += textureLod(srcTexture, TexCoords + vec2(-radius, -radius) * texelSize, srcLod).rgb;
	result += textureLod(srcTexture, TexCoords + vec2(radius, -radius) * texelSize, srcLod).rgb;
	result += textureLod(srcTexture, TexCoords + vec2(-radius, radius) * texelSize, srcLod).rgb;
	result += textureLod(srcTexture, TexCoords + vec2(radius, radius) * texelSize, srcLod).rgb;

	// Cardinal samples at half-pixel offset (weight 2 each)
	result += textureLod(srcTexture, TexCoords + vec2(0.0, -radius) * texelSize, srcLod).rgb * 2.0;
	result += textureLod(srcTexture, TexCoords + vec2(0.0, radius) * texelSize, srcLod).rgb * 2.0;
	result += textureLod(srcTexture, TexCoords + vec2(-radius, 0.0) * texelSize, srcLod).rgb * 2.0;
	result += textureLod(srcTexture, TexCoords + vec2(radius, 0.0) * texelSize, srcLod).rgb * 2.0;

	return vec4(result / 16.0, 1.0);
}


vec4 rejectFireflies(vec4 current, sampler2D rawTex, sampler2D historyTex, sampler2D velocityTex, sampler2D depthTex, vec2 uv, vec2 lowResInvSize) {
	// 1. Spatial Check in raw (recent) frame
	vec4 m1 = vec4(0.0);
	vec4 m2 = vec4(0.0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec4 val = texture(rawTex, uv + vec2(x, y) * lowResInvSize);
			m1 += val;
			m2 += val * val;
		}
	}
	vec4 mean = m1 / 9.0;
	vec4 stddev = sqrt(max(vec4(0.0), (m2 / 9.0) - (mean * mean)));
	vec4 spatialClamp = mean + stddev * 3.0;

	// 2. Temporal Check
	vec2 velocity = texture(velocityTex, uv).rg;
	vec2 prevUV = uv - velocity;
	vec4 history = texture(historyTex, prevUV);

	// Validation: Check if reprojected pixel is on the same surface
	float currentDepth = texture(depthTex, uv).r;
	float prevDepth = texture(depthTex, prevUV).r;
	bool validHistory = (prevUV.x >= 0.0 && prevUV.x <= 1.0 && prevUV.y >= 0.0 && prevUV.y <= 1.0) && (abs(currentDepth - prevDepth) < 0.01);

	// 3. Historical Neighborhood Check
	vec4 hm1 = vec4(0.0);
	vec4 hm2 = vec4(0.0);
	if (validHistory) {
		for (int y = -1; y <= 1; y++) {
			for (int x = -1; x <= 1; x++) {
				vec4 val = texture(historyTex, prevUV + vec2(x, y) * lowResInvSize);
				hm1 += val;
				hm2 += val * val;
			}
		}
	} else {
        // If history is invalid, we fallback to spatial clamp only
        return min(current, spatialClamp);
    }

	vec4 hMean = hm1 / 9.0;
	vec4 hStddev = sqrt(max(vec4(0.0), (hm2 / 9.0) - (hMean * hMean)));
	vec4 historicalClamp = hMean + hStddev * 3.0;

	// Spatial stability in the current frame
	float spatialLum = luminance(mean.rgb);
	float spatialStd = luminance(stddev.rgb);
	float stability = 1.0 - clamp(spatialStd / (spatialLum + 0.001), 0.0, 1.0);

	// If current is significantly brighter than BOTH spatial neighborhood AND historical neighborhood, it's likely a firefly.
	vec4 rejected = min(current, mean + stddev * mix(3.0, 8.0, stability));

	float hLum = luminance(historicalClamp.rgb);
	float currentLum = luminance(current.rgb);

	// Allow more growth if the current signal is spatially stable (i.e., not a single isolated pixel)
	float maxGrowth = mix(5.0, 50.0, stability);
	float maxLum = hLum * maxGrowth + 0.1;

	if (currentLum > maxLum) {
		rejected.rgb *= (maxLum / max(0.001, currentLum));
	}

	return rejected;
}

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
			// Guarantee the center pixel always contributes,
			// preventing absolute zero accumulation on sharp edges.
			if (x == 0 && y == 0) {
				weight = max(weight, 0.1);
			}

			sumColor += color * weight;
			sumWeight += weight;
		}
	}

	if (sumWeight < 0.001) {
		return texture(lowResTex, uv);
	}

	return sumColor / sumWeight;
}


void sampleWithWeights(sampler2D srcTexture, sampler2D highResDepth, sampler2D highResNormal, float centerDepth, vec3 centerNormal, vec2 uv, vec2 offset, inout vec4 color, inout float weight) {
	const float depthSigma = 0.05;
	const float normalSigma = 16.0;

	vec2 offsetUV = uv + offset;

	vec3 rawNormal = texture(highResNormal, offsetUV).xyz;
	vec3 sampleNormal = dot(rawNormal, rawNormal) > 0.001 ? normalize(rawNormal) : vec3(0.0, 1.0, 0.0);
	vec4 sampleColor = texture(srcTexture, offsetUV);
	float sampleDepth = texture(highResDepth, offsetUV).r;

	float spatialW = exp(-float(dot(offset, offset)) / 2.0);
	float depthDiff = abs(centerDepth - sampleDepth);
	float depthW = exp(-(depthDiff * depthDiff) / (2.0 * depthSigma * depthSigma));
	float normalW = pow(max(dot(centerNormal, sampleNormal), 0.0), normalSigma);

	float sampleWeight = spatialW * depthW * normalW;
	if (offset.x == 0 && offset.y == 0) {
		sampleWeight = max(sampleWeight, 0.1);
	}

	weight += sampleWeight;
	color += sampleColor * sampleWeight;
}

vec4 tentUpsample(sampler2D srcTexture, sampler2D highResDepth, sampler2D highResNormal, float filterRadius, float srcLod, vec2 TexCoords) {
	vec2 srcResolution = textureSize(srcTexture, 0);
	filterRadius = max(1.0, filterRadius);
	srcLod = max(0.0, srcLod);

	vec2  texelSize = 1.0 / srcResolution;
	float radius = filterRadius;

	float centerDepth = texture(highResDepth, TexCoords).r;
	vec3 centerNormal = normalize(texture(highResNormal, TexCoords).xyz);

	vec4 result = vec4(0.0);
	float weight = 0.0;

	sampleWithWeights(srcTexture, highResDepth, highResNormal, centerDepth, centerNormal, TexCoords, vec2(0), result, weight);

	sampleWithWeights(srcTexture, highResDepth, highResNormal, centerDepth, centerNormal, TexCoords, vec2(-radius, -radius) * texelSize, result, weight);
	sampleWithWeights(srcTexture, highResDepth, highResNormal, centerDepth, centerNormal, TexCoords, vec2(radius, -radius) * texelSize, result, weight);
	sampleWithWeights(srcTexture, highResDepth, highResNormal, centerDepth, centerNormal, TexCoords, vec2(-radius, radius) * texelSize, result, weight);
	sampleWithWeights(srcTexture, highResDepth, highResNormal, centerDepth, centerNormal, TexCoords, vec2(radius, radius) * texelSize, result, weight);

	sampleWithWeights(srcTexture, highResDepth, highResNormal, centerDepth, centerNormal, TexCoords, vec2(0.0, -radius) * texelSize, result, weight);
	sampleWithWeights(srcTexture, highResDepth, highResNormal, centerDepth, centerNormal, TexCoords, vec2(0.0, radius) * texelSize, result, weight);
	sampleWithWeights(srcTexture, highResDepth, highResNormal, centerDepth, centerNormal, TexCoords, vec2(-radius, 0.0) * texelSize, result, weight);
	sampleWithWeights(srcTexture, highResDepth, highResNormal, centerDepth, centerNormal, TexCoords, vec2(radius, 0.0) * texelSize, result, weight);


	if (weight < 0.001) {
		return texture(srcTexture, TexCoords);
	}

	return result / weight;
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

	ivec2 lowResSize = textureSize(uGIAOTexture, 0);
	vec2 lowResInvSize = 1.0 / vec2(lowResSize);

	// Use bilateral upsampling for low-res effects
	// vec4 giao = sampleBilateral(uGIAOTexture, uDepthTexture, uNormalTexture, TexCoords);
	// float sssFactor = sampleBilateral(uSSSTexture, uDepthTexture, uNormalTexture, TexCoords).r;
	// vec3 di_lighting = sampleBilateral(uDITexture, uDepthTexture, uNormalTexture, TexCoords).rgb;

	vec4 giao =        tentUpsample(uGIAOTexture, uDepthTexture, uNormalTexture, 5.0, 0.0, TexCoords);
	float sssFactor =  tentUpsample(uSSSTexture,  uDepthTexture, uNormalTexture, 5.0, 0.0, TexCoords).r;
	vec3 di_lighting = tentUpsample(uDITexture,   uDepthTexture, uNormalTexture, 5.0, 0.0, TexCoords).rgb;

    // di_lighting += tentUpsample(uDITexture, 5.0, 0.0, TexCoords).rgb;
	// // Basic firefly rejection: clamp ReSTIR contribution based on scene luminance
	// vec4 gR = textureGather(uSceneTexture, TexCoords, 0);
	// vec4 gG = textureGather(uSceneTexture, TexCoords, 1);
	// vec4 gB = textureGather(uSceneTexture, TexCoords, 2);

	// vec3 gAvg = vec3(length(gR)+di_lighting.r, length(gG)+di_lighting.g, length(gB)+di_lighting.b)/4.0;

	// float max_lum = max(gAvg.r, max(gAvg.g, gAvg.b));
	// giao.rgb = min(giao.rgb, vec3(max_lum));
	// di_lighting = min(di_lighting, vec3(max_lum));

	// Advanced firefly rejection
	// giao = rejectFireflies(giao, uRawGIAOTexture, uHistoryGIAOTexture, uVelocityTexture, uDepthTexture, TexCoords, lowResInvSize);
	// di_lighting = rejectFireflies(vec4(di_lighting, 1.0), uRawDITexture, uHistoryDITexture, uVelocityTexture, uDepthTexture, TexCoords, lowResInvSize).rgb;

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
