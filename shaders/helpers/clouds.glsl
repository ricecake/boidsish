#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "../lighting.glsl"
#include "fast_noise.glsl"
#include "math.glsl"
#include "lygia/generative/random.glsl"


float cloudPhase(float cosTheta) {
	// Dual-lobe Henyey-Greenstein for forward and back scattering
	// Blended with a large isotropic component to ensure visibility at all angles
	float hg = mix(henyeyGreenstein(cloudPhaseG1, cosTheta), henyeyGreenstein(cloudPhaseG2, cosTheta), cloudPhaseAlpha);
	return mix(hg, 1.0 / (4.0 * PI), cloudPhaseIsotropic);
}

float beerPowder(float d, float local_d) {
	// Approximation of multiple scattering (Beer-Powder law)
	// Ensuring sunny side isn't black when d is small
	return max(
		exp(-d),
		exp(-d * cloudPowderScale) * cloudPowderMultiplier * (1.0 - exp(-local_d * cloudPowderLocalScale))
	);
}

struct CloudProperties {
	float altitude;
	float thickness;
	float densityBase;
	float coverage;
	float worldScale;
};

struct CloudWeather {
	float weatherMap; // Density/Coverage
	float heightMap;  // Vertical expansion and variety
	float cellID;     // Per-cell variety
};

struct CloudLayer {
	float baseFloor;
	float baseCeiling;
	float thickness;
};

// Warp cloud position away from the camera's view axis (capsule-based sliding warp)
// Returns the warped position and a fade factor for density
vec3 getWarpedCloudPos(vec3 p, out float fade) {
	fade = 1.0;
	return p;
	if (cloudWarp <= 0.0)
		return p;

	// vec3  relP = p - viewPos;
	// float projection = dot(relP, viewDir);

	// Capsule distance: distance to the forward ray starting at viewPos
	// vec3  axisPoint = viewPos + viewDir * max(0.0, projection);
	// vec3  toP = p - axisPoint;
	// float d = length(toP);
	float R = cloudWarp * worldScale;

	// New uniform or constant for how far the bubble extends
	float capsuleLength = cloudWarp * worldScale * 5.0; // Example ratio
	vec3  ap = p - viewPos;
	// t is the projection of the current point onto the view direction
	float t = dot(ap, viewDir);
	// Clamp the projection to the segment bounds [0, capsuleLength]
	float t_clamped = clamp(t, 0.0, capsuleLength);
	// Find the closest point on the clamped segment
	vec3 axisPoint = viewPos + viewDir * t_clamped;
	// Vector from the closest point to the actual point
	vec3 toP = p - axisPoint;
	// d is now the distance to a capsule core, rather than a cylinder core
	float d = length(toP);

	// To "push" clouds out, we sample from a position CLOSER to the axis.
	// This maps the region [R, inf] to [0, inf].
	// float d_sampling = max(0.0, d - R);
	float d_sampling = d * ((d * d) / (d * d + R * R));
	// float d_sampling = d * (1.0 - exp(-d / R));
	// float d_sampling = d * (d / (d + R));
	float scale = d_sampling / max(d, 0.0001);

	// Fade out density in the inner core to create a clean hole and avoid sampling artifacts
	fade = smoothstep(R * 0.1, R, d);
	// fade = 1;

	return axisPoint + toP * scale;
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props) {
	p += vec3(150 * time, 0, 25 * time);
	vec2  weatherData = fastWorley3dID(vec3(p.x, 0.0, p.z) / (10000.0 * worldScale));
	float weatherMap = 1.0 - weatherData.x; // Worley distance for coverage
	float cellID = weatherData.y;           // Cell ID for variety

	p.x *= weatherData.x;
	p.y *= weatherData.y;
	p.z *= weatherData.x;

	float heightMap = fastWorley3d(vec3(p.x, p.y+(3.0 * time), p.z) / (7500.0 * worldScale)) * 0.5 + 0.5;

	CloudWeather weather;
	weather.weatherMap = weatherMap;
	weather.heightMap = heightMap;
	weather.cellID = cellID;

	return weather;
}

CloudLayer computeCloudLayer(CloudWeather weather, CloudProperties props) {
	// Use heightMap for dramatic vertical expansion for specific weather cells
	// Tall clouds (cumulonimbus) can be 5-10x thicker than base thickness
	float verticalExpansion = mix(1.0, 8.0, weather.heightMap * weather.weatherMap);

	CloudLayer layer;
	layer.baseFloor = props.altitude * props.worldScale;
	layer.baseCeiling = (props.altitude + props.thickness * verticalExpansion) * props.worldScale;
	layer.thickness = max(layer.baseCeiling - layer.baseFloor, 0.001);
	return layer;
}

vec3 getCloudAdvectionOffset(float h, float worldScale, float time) {
	float angle = cloudFlowDirection;
	vec2  flowDir = vec2(cos(angle), sin(angle));

	// Dramatic non-linear shear profile
	float shear = h * h * cloudFlowHeightScale * 2.0;

	vec3 advect = vec3(flowDir.x, 0.0, flowDir.y) * time * cloudFlowSpeed * worldScale * 10.0;
	advect.xz += flowDir * shear * worldScale * 1000.0;

	return advect;
}

vec3 erot(vec3 p, vec3 ax, float ro) {
    return mix(dot(p,ax)*ax,p,cos(ro))+sin(ro)*cross(ax,p);
}

float WaveletNoise(vec3 p, float z, float k) {
    // https://www.shadertoy.com/view/wsBfzK
    float d=0.,s=1.,m=0., a;
    for(float i=0.; i<5.; i++) {
        vec3 q = p*s, g=fract(floor(q)*vec3(123.34,233.53,314.15));
    	g += dot(g, g+23.234);
		a = fract(g.x*g.y)*1e3 +z*(mod(g.x+g.y, 2.)-1.); // add vorticity
        q = (fract(q)-.5);
        //random rotation in 3d. the +.1 is to fix the rare case that g == vec3(0)
        //https://suricrasia.online/demoscene/functions/#rndrot
        q = erot(q, normalize(tan(g+.1)), a);
        d += sin(q.x*10.+z)*smoothstep(.25, .0, dot(q,q))/s;
        p = erot(p,normalize(vec3(-1,1,0)),atan(sqrt(2.)))+i; //rotate along the magic angle
        m += 1./s;
        s *= k;
    }
    return d/m;
}


// Cloud density calculation helper
// Returns a density value [0, 1+] based on world-space position
float calculateCloudDensity(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
) {
	if (p.y < layer.baseFloor || p.y > layer.baseCeiling)
		return 0.0;

	float coverageThreshold = 1.0 - props.coverage;

	// p.y /= (p.y - layer.baseFloor) / layer.thickness;
	p.y -= weather.heightMap * layer.thickness;


	// p.y /= 1.0/remap(p.y, layer.baseFloor, layer.baseCeiling, 0.0, 1.0);
	// p.y = remap(weather.heightMap, 0.0, 1.0, layer.baseFloor, layer.baseCeiling);
	// p.y /= remap(weather.heightMap, p.y, layer.baseCeiling, 0.0, 1.0);

	// Height-based tapering with a more natural profile
	float h = (p.y - layer.baseFloor) / layer.thickness;
	float tapering = smoothstep(0.0, 0.15, h) * 1.0-smoothstep(0.7, 1.0, h);


	// Apply advection to the sample position
	vec3 advect = vec3(0);//getCloudAdvectionOffset(h, props.worldScale, time);
	vec3 p_advected = p + advect;

	// Base noise for cloud shapes
	vec3 p_warped = p;
/*
	vec3 p_scaled = (p_advected) / (1000.0 * props.worldScale);

	vec2 baseBubble = fastWorley3dID(p_scaled);
	float cloudFactor = baseBubble.y;
	// vec3 p_scaled_adv = (p_advected +time*cloudFactor) / (50000.0 * props.worldScale);
	// float baseNoise = (fastWorley3d(p_scaled));
	// float baseNoise = abs((fastSimplex3d(p_scaled_adv)) + baseBubble.x);
	// float baseNoise = baseBubble.x;
	// float baseNoise = 1.0-baseBubble.x;
	// float baseNoise = fastFbmCurl3d(p_scaled_adv)-(1.0-baseBubble.x);
	// float baseNoise = fastPhasor2d(random2(baseBubble.y), degrees(0))*baseBubble.x;
	// float baseNoise = WaveletNoise(p_warped/2000, 1.52, degrees(cloudFactor*time))*baseBubble.x;
	float baseNoise = baseBubble.x;// - (fastSimplex3d(p_scaled_adv));
	// baseNoise = cloudFactor*smoothstep(0, 0.85, baseNoise);
	// return smoothstep(coverageThreshold, max(1.0, coverageThreshold), baseNoise);
	// return 1.0 * step(coverageThreshold, weather.weatherMap * cloudFactor);
	// return baseNoise * step(coverageThreshold, remap(weather.weatherMap, props.densityBase*cloudFactor, 1.0, 0.0, 1.0));
	return baseNoise*weather.weatherMap * step(coverageThreshold, cloudFactor);
*/
	vec3 p_scaled = (p_advected) / (8000.0 * props.worldScale);
	vec2 baseBubble = fastWorley3dID(p_scaled);

	float cloudFactor = baseBubble.y;

	float weight = smoothstep(coverageThreshold, 1.0, baseBubble.y);
	// float baseNoise = smoothstep(coverageThreshold, 1.0, baseBubble.x);
	float baseNoise = weight * step(0, baseBubble.y - baseBubble.x);
/*
	for (uint i = 4; i <=6; i++) {
		vec3 scaled_p = (p_advected) / (pow(6, i) * props.worldScale);
		vec2 bubble = fastWorley3dID(scaled_p);
		// baseNoise += step(coverageThreshold, bubble.y) * step(coverageThreshold, bubble.x);
		float stepWeight = step(coverageThreshold, bubble.y);
		baseNoise +=  stepWeight * smoothstep(coverageThreshold, 1.0, bubble.x);
		weight += stepWeight;
	}
*/
	for (uint i = 4; i <=6; i++) {
		vec3 scaled_p = (p_advected) / (pow(6, i) * props.worldScale);
		vec2 bubble = fastWorley3dID(scaled_p);
		float stepWeight = smoothstep(coverageThreshold, 1.0, bubble.y);
		baseNoise += stepWeight*step(0, bubble.y - bubble.x);
		weight += stepWeight;
	}

	return smoothstep(coverageThreshold-0.05, 0.750, baseNoise/weight);
	// baseNoise = smoothstep(coverageThreshold, 1.0, baseNoise/weight);

	// Implement "Roll": Billowy edges that vary with height
	// We remap the base noise threshold based on the vertical position
	float rollFactor = remap(h, 0.0, 1.0, 0.4, 0.1);
	float rolledNoise = remap(baseNoise, rollFactor, 1.0, 0.0, 1.0);

	// Tall cloud profile: anvil-like top for tall clouds
	// Mix between a bottom-heavy profile and an anvil profile based on heightMap
	float bottomHeavy = tapering;
	float anvil = pow(tapering, mix(0.7, 0.3, weather.heightMap));
	float densityProfile = mix(bottomHeavy, anvil, ((cloudFactor + 0.5) * h) * weather.heightMap);

	// Add ridges and textures for definition
	vec3 slide = p_warped;
	slide.xz += cloudFactor*time*25.0;
	float ridges = fastRidge3d(slide / (1600.0 * props.worldScale));
	float detail = fastFbm3d(slide / (1450.0 * props.worldScale));

	// Combine noises
	float finalNoise = rolledNoise * (0.6 + 0.4 * ridges);
	finalNoise = mix(finalNoise, remap(finalNoise, detail, 1.0, 0.0, 1.0), 0.3);

	// Apply coverage and local density
	float baseDensity =  finalNoise * weather.weatherMap;

	// Add "Edge Wisps": high-frequency FBM at the boundaries
	if (baseDensity > 0.0 && baseDensity < 0.3) {
		float wisps = fastFbm3d((p_warped+time*(30.0)) / (1000.0 * props.worldScale));
		float wispMask = 1.0 - smoothstep(0.0, 0.5, baseDensity);
		baseDensity += wisps * wispMask * 0.35 * weather.weatherMap;
	}

	// Giant tall clouds vs wispy things
	// High weatherMap = tall, dense, sharp
	// Low weatherMap = wispy, thin, soft
	float wispyFactor = smoothstep(0.2, 0.35, weather.weatherMap);
	baseDensity *= mix(0.6, 1.0, wispyFactor);
	float density = smoothstep(coverageThreshold, max(1.0, coverageThreshold), baseDensity);

	return smoothstep(0, 0.75, density * densityProfile * props.densityBase * 5.0);
/*

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
		float density = smoothstep(coverageThreshold, max(1.0, coverageThreshold), rolledNoise * weather.weatherMap);
		return smoothstep(0, 0.65, density * densityProfile * props.densityBase * 5.0);
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
	float baseDensity =  finalNoise * weather.weatherMap;

	// Add "Edge Wisps": high-frequency FBM at the boundaries
	if (baseDensity > 0.0 && baseDensity < 0.3) {
		float wisps = fastFbm3d((p_warped+time*(30.0)) / (1000.0 * props.worldScale));
		float wispMask = 1.0 - smoothstep(0.0, 0.5, baseDensity);
		baseDensity += wisps * wispMask * 0.35 * weather.weatherMap;
	}

	// Giant tall clouds vs wispy things
	// High weatherMap = tall, dense, sharp
	// Low weatherMap = wispy, thin, soft
	float wispyFactor = smoothstep(0.2, 0.35, weather.weatherMap);
	baseDensity *= mix(0.6, 1.0, wispyFactor);
	float density = smoothstep(coverageThreshold, max(1.0, coverageThreshold), baseDensity);

	return smoothstep(0, 0.75, density * densityProfile * props.densityBase * 5.0);
*/
}

float calculateCloudShadowDensity(vec3 p, CloudWeather weather, CloudLayer layer, CloudProperties props, float time) {
	return 10.0 * calculateCloudDensity(p, weather, layer, props, time, true);
}

/**
 * High-level function to evaluate cloud shadow density at a specific world XZ position.
 * This encapsulates the logic used by both the shadow map generator and the runtime fallback.
 */
float evaluateCloudShadowDensityAtWorldPos(vec2 worldXZ, float time) {
	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	// Use a dummy cloud position to evaluate weather/layer at this XZ
	vec3  basePos = vec3(worldXZ.x, props.altitude * props.worldScale, worldXZ.y);
	CloudWeather weather = computeCloudWeather(basePos, props);
	CloudLayer layer = computeCloudLayer(weather, props);

	// Integrate density vertically through the expanded layer to capture the full shadow
	float totalDensity = 0.0;
	const int shadowSteps = 4;
	float stepSize = layer.thickness / float(shadowSteps);

	for (int i = 0; i < shadowSteps; i++) {
		vec3 p = vec3(worldXZ.x, layer.baseFloor + (float(i) + 0.5) * stepSize, worldXZ.y);
		totalDensity += calculateCloudDensity(p, weather, layer, props, time, true);
	}

	return totalDensity * stepSize * 0.1; // Scale to representative optical depth
}

#endif // HELPERS_CLOUDS_GLSL
