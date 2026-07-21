#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "../lighting.glsl"
#include "fast_noise.glsl"
#include "math.glsl"
#include "lygia/generative/random.glsl"
// #include "lygia/generative.glsl"
// #include "lygia/generative/snoise.glsl"
#include "lygia/sdf.glsl"
#include "cloud_utils.glsl"

vec4 calculateCloudDensityHZDv1(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
){
	float h = (p.y - layer.baseFloor) / layer.thickness;

	// Height-based density gradient (cloud type)
	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);

	// Apply advection
	vec3 advect = getCloudAdvectionOffset(h, time);
	vec3 p_advected = p + advect;

	// Domain warping for "boiling" look (using curl noise)
	if (!simplified) {
		vec3 curl = fastCurl3d(p_advected / (cloudCurlFrequency * props.worldScale * 5000.0));
		p_advected += curl * cloudCurlStrength * props.worldScale * 5000.0 * (1.0 - h);
	}

	// Base noise sampling (Perlin-Worley hybrid proxy)
	vec3 p_scaled = p_advected / (30000.0 * props.worldScale);
	float perlin = (fastFbm3d(p_scaled) + 1.0) * 0.5;
	float worley = fastWorley3d(p_scaled);

	// Perlin-Worley hybrid: remap perlin by worley
	float baseNoise = remapClamp(perlin, worley, 1.0, 0.0, 1.0);

	// Apply height gradient
	baseNoise *= heightGradient;

	// Coverage remapping
	float coverage = props.coverage * getCloudCoverageFromSDF(weather.sdf, props.worldScale);
	float baseDensity = remapClamp(baseNoise, 1.0 - coverage, 1.0, 0.0, 1.0);
	baseDensity *= coverage;

	if (simplified) {
		return vec4(clamp(baseDensity * props.densityBase * 2.0, 0.0, 1.0), advect);
	}

	// Detail erosion
	vec3 p_detail = p_advected / (4500.0 * props.worldScale);
	float detailWorley = fastWorley3d(p_detail);

	// Erode more at the bottom for "wispy" look, less at the top for "bulgy" look
	float erosion = detailWorley * (1.0 - h);
	// float finalDensity = remapClamp(baseDensity, erosion * 0.4, 1.0, 0.0, 1.0);

	float coverageThreshold = 1.0 - props.coverage;

	// return clamp(finalDensity * props.densityBase * 2.0, 0.0, 2.0);
	// return smoothstep(coverageThreshold, 1.0, baseDensity) * remap(baseDensity * 2.0, erosion * 0.4, 1.0, 0.0, props.densityBase);
	float finalDensity = remap(baseDensity * 2.0, erosion * 0.4, 1.0, 0.0, 2.0*props.densityBase);
	return vec4(smoothstep(coverageThreshold, 1.0, finalDensity) * finalDensity, advect);
}

vec4 calculateCloudDensityExpV1(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
) {
	float h = (p.y - layer.baseFloor) / layer.thickness;

	// Height-based density gradient (cloud type)
	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);

	// Apply advection
	vec3 advect = getCloudAdvectionOffset(h, time);
	vec3 p_advected = p + advect;
	vec3 p_scaled = p_advected / (3000.0 * props.worldScale);


	float baseNoise = fastWorley3d((p_advected)/10000.0);
	baseNoise *= 1.0-smoothstep(-0.10, props.coverage, baseNoise);
	float erosion = (fastFbm3d(p_scaled) + 1.0) * 0.5;
	baseNoise = remapClamp(baseNoise, erosion * 0.4, 1.0, 0.0, props.densityBase);
	return vec4(baseNoise, advect);

}


vec4 calculateCloudDensityExpV2(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
) {
	if (p.y < layer.baseFloor || p.y > layer.baseCeiling)
		return vec4(0.0);

	// p.y -= weather.heightMap * layer.thickness;

	// Height-based tapering with a more natural profile
	float h = (p.y - layer.baseFloor) / layer.thickness;
	float tapering = smoothstep(0.0, 0.15, h) * 1.0-smoothstep(0.7, 1.0, h);

	float coverageThreshold = 1.0 - props.coverage;

	// Apply advection to the sample position
	vec3 advect = getCloudAdvectionOffset(h, time);
	vec3 p_advected = p + advect;

	// Base noise for cloud shapes
	vec3 p_warped = p;
	vec3 p_scaled = (p_advected) / (50000.0 * props.worldScale);

	vec2 baseBubble = fastWorley3dID(p_scaled);
	float cloudFactor = baseBubble.y;

	return vec4(step(coverageThreshold, baseBubble.x) * step(0.00, cloudFactor), advect);

	vec3 p_scaled_adv = (p_advected +time*cloudFactor) / (50000.0 * props.worldScale);
	// float baseNoise = (fastWorley3d(p_scaled));
	// float baseNoise = abs((fastSimplex3d(p_scaled_adv)) + baseBubble.x);
	// float baseNoise = baseBubble.x;
	float baseNoise = 1.0-baseBubble.x;
	// float baseNoise = fastFbmCurl3d(p_scaled_adv)-(1.0-baseBubble.x);
	// float baseNoise = fastPhasor2d(random2(baseBubble.y), degrees(0))*baseBubble.x;
	// float baseNoise = WaveletNoise(p_warped/2000, 1.52, degrees(cloudFactor*time))*baseBubble.x;


	// Implement "Roll": Billowy edges that vary with height
	// We remap the base noise threshold based on the vertical position
	float rollFactor = remap(h, 0.0, 1.0, 0.4, 0.1);
	float rolledNoise = remap(baseNoise, rollFactor, 1.0, 0.0, 1.0);

	// Tall cloud profile: anvil-like top for tall clouds
	// Mix between a bottom-heavy profile and an anvil profile based on heightMap
	float bottomHeavy = tapering;
	float anvil = pow(tapering, mix(0.7, 0.3, weather.heightMap));
	float densityProfile = mix(bottomHeavy, anvil, ((cloudFactor + 0.5) * h) * weather.heightMap);

	if (simplified) {
		float coverage = getCloudCoverageFromSDF(weather.sdf, props.worldScale);
		float density = smoothstep(coverageThreshold, max(1.0, coverageThreshold), rolledNoise * coverage);
		return vec4(smoothstep(0, 0.65, density * densityProfile * props.densityBase * 5.0), advect);
	}

	// Add ridges and textures for definition
	vec3 slide = p_warped;
	slide.xz += cloudFactor*time*25.0;
	float ridges = fastRidge3d(slide / (1600.0 * props.worldScale));
	float detail = fastFbm3d(slide / (1450.0 * props.worldScale));

	// Combine noises
	float finalNoise = rolledNoise * (0.6 + 0.4 * ridges);
	finalNoise = mix(finalNoise, remap(finalNoise, detail, 1.0, 0.0, 1.0), 0.3);

	// Apply coverage and local density
	float baseDensity =  finalNoise * getCloudCoverageFromSDF(weather.sdf, props.worldScale);

	// Add "Edge Wisps": high-frequency FBM at the boundaries
	if (baseDensity > 0.0 && baseDensity < 0.3) {
		float coverage = getCloudCoverageFromSDF(weather.sdf, props.worldScale);
		float wisps = fastFbm3d((p_warped+time*(30.0)) / (1000.0 * props.worldScale));
		float wispMask = 1.0 - smoothstep(0.0, 0.5, baseDensity);
		baseDensity += wisps * wispMask * 0.35 * coverage;
	}

	// Giant tall clouds vs wispy things
	// High coverage = tall, dense, sharp
	// Low coverage = wispy, thin, soft
	float coverage = getCloudCoverageFromSDF(weather.sdf, props.worldScale);
	float wispyFactor = smoothstep(0.2, 0.35, coverage);
	baseDensity *= mix(0.6, 1.0, wispyFactor);

	float density = smoothstep(coverageThreshold, max(1.0, coverageThreshold), baseDensity);

	return vec4(smoothstep(-0.1, 1.0, density * densityProfile * props.densityBase * 2.0), advect);
}

vec4 calculateCloudDensityExpV3(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
) {
	if (p.y < layer.baseFloor || p.y > layer.baseCeiling)
		return vec4(0.0);

	// p.y -= weather.heightMap * layer.thickness;

	// Height-based tapering with a more natural profile
	float h = (p.y - layer.baseFloor) / layer.thickness;
	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);

	float tapering = smoothstep(0.0, 0.15, h) * 1.0-smoothstep(0.7, 1.0, h);

	float coverageThreshold = 1.0 - props.coverage;

	// Apply advection to the sample position
	vec3 advect = getCloudAdvectionOffset(h, time);
	vec3 p_advected = p;// - advect;
	vec3 p_deadvected = p - advect;
	vec3 p_warp = p - advect;
	vec3 p_scaled = (p_advected) / (50000.0 * props.worldScale);
	vec3 p_descaled = (p_deadvected) / (50000.0 * props.worldScale);


	// vec2 worley = fastWorley3dID(p_scaled) + 0.5 * fastWorley3dID(p_scaled * 2.0)+ 0.25 * fastWorley3dID(p_scaled * 4.0);

	// return (cos(2*worley.y*time)+1.5)*step(sin(time*worley.y)*0.5+0.5, worley.x);
	// return (cos(2*worley.y*time)+1.5)*step(sin(time*worley.y)*0.5+0.5, length(fract(p)));

	float coverage = getCloudCoverageFromSDF(weather.sdf, props.worldScale);
	// float baseNoise = smoothstep(coverageThreshold, 1.0, max(worley.x,worley.y));
	float baseNoise = smoothstep(coverageThreshold, 1.0, coverage);// * max(worley.x,worley.y);
	// float baseNoise = remap(max(worley.x,worley.y), coverageThreshold, 1.0, 0.0, 1.0);
	// float baseNoise = remap(remap(worley.y, worley.x, 1.0, 0.0, 1.0), coverageThreshold, 1.0, 0.0, 1.0);

	// if (simplified) {
	// 	return vec4(remapClamp(baseNoise*heightGradient*2.0, 0, 1.0, 0.0, props.densityBase), advect);
	// }

	vec3 p_warped = (p_deadvected+10*fastCurl3d((p_warp) / (50000.0 * props.worldScale))) / (1000.0 * props.worldScale);

	vec2 worley = vec2(0);
	for (float i = 0; i < 3; i++) {
		worley += pow(2, -i) * fastWorley3dID(p_descaled * pow(2, i));
	}

	float ridge = abs(fastRidge3d(p_descaled*5));
	float fbm = fastWarpedFbm3d(p_warped/3);

	// return vec4(step(coverageThreshold, worley.x*step(coverageThreshold, worley.y)) * remapClamp(baseNoise, mix(worley.x, fastFbmCurl3d(p_scaled), h) * 0.5, 1.0, 0.0, props.densityBase), advect);
	// float val = remapClamp(baseNoise*heightGradient*2, mix(max(worley.y, worley.x), fbm, smoothstep(0.25, 0.75, h)), 1.0, 0.0, props.densityBase);
	float val = remapClamp(baseNoise*heightGradient*2, mix(ridge, fbm, smoothstep(0.25, 0.75, h)), 1.0, 0.0, props.densityBase);
	return vec4(2.0*val * step(0.3, val), advect);
	// return vec4(5.0*remapClamp(baseNoise*heightGradient*2, mix(worley.x, 2*fbm*ridge, h), 1.0, 0.0, props.densityBase), advect);
}


vec4 calculateCloudDensityExpV4(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
) {
	float h = (p.y - layer.baseFloor) / layer.thickness;

	float heightGrid = 150+250*(mix(500, 1000, h)/250);

	vec3 roundP = heightGrid*round((p/heightGrid));
	float dist = distance(p, roundP);
	float baseNoise = step(dist, 100);
	float erosion = (fastFbm3d(p/1000) + 1.0) * 0.5;
	baseNoise = remapClamp(baseNoise, erosion * 0.4, 1.0, 0.0, props.densityBase);

	return vec4(baseNoise, vec3(0.0));// * erosion;
}

vec4 calculateCloudDensityExpV5(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
) {
	float h = (p.y - layer.baseFloor) / layer.thickness;
	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);

	float coverageThreshold = 1.0 - props.coverage;

	vec3 advect = getCloudAdvectionOffset(h, time);
	vec3 p_advected = p + advect;
	vec3 p_scaled = (p_advected) / (50000.0 * props.worldScale);
	vec2 worley = fastWorley3dID(p_scaled*5.0);

	float baseNoise = remapClamp(fastSimplex3d(p_scaled), worley.x, 1.0, 0.0, 1.0);
	baseNoise *= heightGradient;

	float coverage = props.coverage * getCloudCoverageFromSDF(weather.sdf, props.worldScale);
	float baseDensity = remapClamp(baseNoise, 1.0 - coverage, 1.0, 0.0, 1.0);
	// baseDensity *= coverage;

	// return vec4(step(coverageThreshold, worley.x*step(coverageThreshold, worley.y)) * remapClamp(baseNoise, mix(worley.x, fastFbmCurl3d(p_scaled), h) * 0.5, 1.0, 0.0, props.densityBase), advect);
	// float finalNoise = remapClamp(baseNoise, worley.x, 1.0, coverage, props.densityBase);
	// float finalNoise = remap(baseDensity * 2.0, ((fastRidge3d(p_scaled * h * 100.0)+1.0)*0.5)*0.4, 1.0, 0.0, 2.0*props.densityBase);
	float finalNoise = remap(baseDensity, ((fastRidge3d(p_scaled * h * 100.0)*0.5)+0.5)*0.25, 1.0, 0.0, props.densityBase);
	// float finalNoise = baseDensity;
	return vec4(finalNoise, advect);
}

vec4 calculateCloudDensityExpV6(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
) {
	float h = (p.y - layer.baseFloor) / layer.thickness;
	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);

	float tapering = smoothstep(0.0, 0.15, h) * 1.0-smoothstep(0.7, 1.0, h);
	float coverageThreshold = 1.0 - props.coverage;

	// // Apply advection to the sample position
	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	vec3 advect = time * advectSpeed;
	vec3 p_advected = p + advect;
	// vec3 p_scaled = (p_advected) / (50000.0 * props.worldScale);

	// Domain warping for "boiling" look (using curl noise)
	vec4 rawCurlData = textureLod(u_curlTexture, (p_advected-advect) / (cloudCurlFrequency * props.worldScale * 10000.0), 0.0);
	// vec3 curl = fastCurl3d(p_advected / (cloudCurlFrequency * props.worldScale * 5000.0));
	p_advected += rawCurlData.rgb * 10 * props.worldScale * 5000.0 * (1.0 - h);

	// Base noise sampling (Perlin-Worley hybrid proxy)
	vec3 p_scaled = p_advected / (50000.0 * props.worldScale);
	vec2 worleyis = fastWorley3dID(p_scaled);
	// float perlin = (fastFbm3d(p_scaled) + 1.0) * 0.5;
	// float worley = fastWorley3d(p_scaled);
	float perlin = worleyis.y;
	float worley = worleyis.x;

	// Perlin-Worley hybrid: remap perlin by worley
	// float baseNoise = remapClamp(perlin, mix(worley, rawCurlData.a, h), 1.0, 0.0, 1.0);
	float baseNoise = remapClamp(perlin, worley, 1.0, 0.0, 1.0);

	// Apply height gradient
	baseNoise *= heightGradient;

	// Coverage remapping
	float coverageFromSDF = getCloudCoverageFromSDF(weather.sdf, props.worldScale);
	float coverage = props.coverage * step(0.25, coverageFromSDF);
	float baseDensity = remapClamp(baseNoise+coverageFromSDF, 1.0 - coverage, 1.0, 0.0, props.densityBase);
	baseDensity *= coverage;


	return vec4(baseDensity, advectSpeed);// * smoothstep(coverageThreshold, coverageThreshold+0.01, coverageFromSDF);
}

vec4 calculateCloudDensityExpV7(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
) {
	float h = (p.y - layer.baseFloor) / layer.thickness;
	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	// float type = weather.cellID;
		float type = weather.heightMap;

	float heightGradient = getDensityHeightGradient(h, type);

	vec3 advect = time * advectSpeed;
	vec3 p_advected = p + advect;

	vec4 rawCurlData = textureLod(u_curlTexture, (p_advected-advect) / (cloudCurlFrequency * props.worldScale * 75000.0), 0.0);
	p_advected += rawCurlData.rgb  * props.worldScale * 5000.0 * (1.0 - h);

	vec3 p_scaled = p_advected / (50000.0 * props.worldScale);
	vec2 worleyis = fastWorley3dID(p_scaled);
	float perlin = worleyis.y;
	float worley = worleyis.x;

	float baseNoise = remapClamp(perlin, worley, 1.0, 0.0, 1.0);

	float tapering = smoothstep(0.0, 0.15, h) * 1.0-smoothstep(0.7, 1.0, h);

	baseNoise *= heightGradient;

	float coverageFromSDF = getCloudCoverageFromSDF(weather.sdf-0.5, props.worldScale);
	// float coverage = props.coverage * step(0.25, coverageFromSDF);
	// float baseDensity = remapClamp(baseNoise, 1.0 - coverage, 1.0, 0.0, props.densityBase);

	float map = (1.0-coverageFromSDF) * tapering;
	float coverage = props.coverage * step(0.25, coverageFromSDF);
	float baseDensity = remapClamp((baseNoise+tapering*map), 1.0 - coverage, 1.0, 0.0, props.densityBase);
	baseDensity *= coverage;

	// baseDensity *= tapering;

	return vec4(baseDensity, advectSpeed);
	// return vec4(max(0.0, min(coverageFromSDF, baseDensity)), advectSpeed);
	// return vec4(max(0.0, min(baseNoise, heightGradient*coverageFromSDF)), advectSpeed);
}


vec4 calculateCloudDensityExpV8(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	float           simplified
) {
	float altitudeShift = weather.heightMap * layer.thickness;
	float actualThickness = weather.thickness * layer.thickness;
	float localFloor = layer.baseFloor + altitudeShift;

	// h is normalized over the actual local cloud thickness to correctly apply the height gradient and wind shear
	float altitude = getCurvedAltitude(p);
	float h = clamp((altitude - localFloor) / max(actualThickness, 0.001), 0.0, 1.0);

	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);
	vec3 advect = time * advectSpeed;
	vec3 p_advected = p + advect;

	float warpy = fastFbm3d(p_advected/5000.0);
	vec3 warpOffset = vec3(warpy) * 1500.0 * props.worldScale;
	// float baseSdf = calculatePuffyCloudSDF(p + warpOffset, weather, layer, props.worldScale);
	float baseSdf = calculateLoftedCloudSDF(p + warpOffset, weather, layer, props.worldScale);

	float d3d = baseSdf;// + (fastRidge3d(p_advected / 15000.0) * 2000.0 * props.worldScale);

	bool isCore = d3d <= -10000.0;
	// float baseNoise = (1.0-0.25*clamp(sin(2*p_advected.x) + sin(5*p_advected.y) + sin(11*p_advected.z), 0, 1)    ) * clamp(-d3d, 0, 1);
	float baseNoise = clamp(-d3d, 0, 1);

	float erodeMask = smoothstep(-10000.0, 0.0, d3d);
	if (erodeMask > 0.0) {
		float largeScale = abs(fastFbm3d(p_advected/10000)) * erodeMask;
		baseNoise = remap(baseNoise, largeScale, 1.0, 0.0, 1.0);

		if (!isCore) {
			if (simplified < 1.0) {
				erodeMask = 1.0 - baseNoise;
				float coarseScale = abs(warpy) * erodeMask;
				baseNoise = remap(baseNoise, coarseScale, 1.0, 0.0, 1.0);
			}

			if (simplified < .75) {
				erodeMask = 1.0 - baseNoise;
				float mediumScale = (1.0-fastRidge3d(p_advected/4000)) * erodeMask;
				baseNoise = remap(baseNoise, mediumScale, 1.0, 0.0, 1.0);
			}

			if (simplified < 0.50) {
				erodeMask = 1.0 - baseNoise;
				float fineScale = abs(fastFbm3d(p_advected / 3000.0)) * erodeMask;
				baseNoise = remap(baseNoise, fineScale, 1.0, 0.0, 1.0);
			}

			if (simplified < 0.25) {
				erodeMask = 1.0 - baseNoise;
				float detailScale = fastRidge3d(p / vec3(2000.0, 1000.0, 2000.0)) * erodeMask;
				baseNoise = remap(baseNoise, detailScale, 1.0, 0.0, 1.0);
			}
		}
		baseNoise *= smoothstep(0.01, 0.02, baseNoise);
	}

	return vec4(clamp(baseNoise, 0.00, 1.0), advectSpeed);
}


struct CloudSpotDetails {
	float density;
	vec3 relativeExtinction;
	vec3 advectionSpeed;
};

struct CloudDensityResult {
	vec3 density;
	vec3 advectionSpeed;
	float ao;
	vec3 albedo;
	vec3 emissivity;
};

CloudSpotDetails calculateCloudDensityExpV9(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	float            simplified
) {
	float h = (p.y - layer.baseFloor) / layer.thickness;
	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);
	vec3 advect = time * advectSpeed;
	vec3 p_advected = p + advect;

	float baseSdf = getCloud3DSDF(p_advected, weather, layer, props.worldScale);
	float baseNoise = clamp(-baseSdf, 0, 1);

	baseNoise *= heightGradient;

	bool isCore = baseNoise >= 1.0;

	float erodeMask = smoothstep(0.0, 1.0, baseNoise);
	if (erodeMask > 0.0 && !isCore) {
		float largeScale = abs(fastFbm3d(p_advected/10000)) * erodeMask;
		baseNoise = adjust(baseNoise, largeScale);

		if (!isCore) {
			if (simplified < 1.0) {
				erodeMask = 1.0 - baseNoise;
				float coarseScale = abs(fastFbm3d(p_advected/5000.0)) * erodeMask;
				baseNoise = adjust(baseNoise, coarseScale);
			}

			if (simplified < .75) {
				erodeMask = 1.0 - baseNoise;
				float mediumScale = (1.0-fastRidge3d(p_advected/4000)) * erodeMask;
				baseNoise = adjust(baseNoise, mediumScale);
			}

			if (simplified < 0.50) {
				erodeMask = 1.0 - baseNoise;
				float fineScale = abs(fastFbm3d(p_advected / 3000.0)) * erodeMask;
				baseNoise = adjust(baseNoise, fineScale);
			}

			if (simplified < 0.25) {
				erodeMask = 1.0 - baseNoise;
				float detailScale = fastRidge3d(p / vec3(2000.0, 1000.0, 2000.0)) * erodeMask;
				baseNoise = adjust(baseNoise, detailScale);
			}
		}
		baseNoise *= smoothstep(0.01, 0.32, baseNoise);
	}

	return CloudSpotDetails(
		clamp(baseNoise, 0.00, 1.0),
		vec3(1.0),
		advectSpeed
	);
}

uniform sampler3D u_cloud3DTexture;

// Cloud density calculation helper
// Returns CloudDensityResult based on world-space position
CloudDensityResult calculateCloudDensityMinimal(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props
) {
	float altitude = getCurvedAltitude(p);
	float h = clamp((altitude - layer.baseFloor) / max(2.0 * layer.thickness, 0.001), 0.0, 1.0);
	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	CloudDensityResult pointDetails = CloudDensityResult(vec3(0.0), advectSpeed, 1.0, vec3(1.0), vec3(0.0));

	if (altitude < layer.baseFloor || altitude > layer.baseCeiling) {
		return pointDetails;
	}

	CloudSpotDetails res = calculateCloudDensityExpV9(p, weather, layer, props, time, 1000.0);

	return CloudDensityResult(res.relativeExtinction, advectSpeed, 1.0, vec3(1.0), vec3(0.0));
}


CloudDensityResult calculateCloudDensity(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	float           simplified
) {
	float altitude = getCurvedAltitude(p);
	float h = clamp((altitude - layer.baseFloor) / max(2.0 * layer.thickness, 0.001), 0.0, 1.0);
	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	CloudDensityResult pointDetails = CloudDensityResult(vec3(0.0), advectSpeed, 1.0, vec3(1.0), vec3(0.0));

	if (altitude < layer.baseFloor || altitude > layer.baseCeiling) {
		return pointDetails;
	}

	// Sample the 3D cloud volume texture with slower advection speed
	vec3 advect_3d = time * advectSpeed * 0.75;
	vec3 p_advected_3d = p + advect_3d;
	vec3 uvw = vec3(
		p_advected_3d.x / (10000.0 * props.worldScale),
		h,
		p_advected_3d.z / (50000.0 * props.worldScale)
	);

	vec4 volSample = textureLod(u_cloud3DTexture, uvw, 0.0);
	float volNoise = volSample.r;
	float volAo = volSample.g;
	// float volAlbedoBasis = volSample.b;

	CloudSpotDetails res = calculateCloudDensityExpV9(p, weather, layer, props, time, simplified);

	// Where the current system has density, the 3d volume adds variety and breaks up the linear nature.
	float finalDensity = res.density;
	if (finalDensity > 0.0) {
		// finalDensity *= mix(0.4, 1.6, volNoise);
		finalDensity = adjust(finalDensity, 1.0-volNoise);
	}

	vec3 mixedDensity = res.relativeExtinction * finalDensity;
	// vec3 mixedAlbedo = vec3(volAlbedoBasis);

	// return CloudDensityResult(mixedDensity, advectSpeed, volAo, mixedAlbedo, smoothstep(0.8, 1.2, mixedDensity) * vec3(0,1,0));
	return CloudDensityResult(mixedDensity, advectSpeed, volAo, vec3(1.0), vec3(0.0));
}

#endif // HELPERS_CLOUDS_GLSL
