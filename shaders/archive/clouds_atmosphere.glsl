float getCloud3DCoverage(vec3 p, CloudWeather weather, CloudLayer layer, float worldScale) {
	float localFloor, actualThickness;
	float h = getCloudRelativeHeight(p, weather, layer, localFloor, actualThickness);

	if (p.y < localFloor || p.y > (localFloor + actualThickness)) {
		return 0.0;
	}

	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);

	float coverage2D = weather.coverage; //

	// // Create a flare modifier that increases in the upper half of the cloud.
	// // Adjust the smoothstep bounds and multiplier to control the flare's altitude and width.
	// float topFlare = smoothstep(0.4, 0.9, h) * 0.4;

	// // Apply the flare to the 2D coverage, clamping to keep it a valid SDF/mask.
	// float dynamicCoverage = clamp(coverage2D + topFlare, 0.0, 1.0);

	// // Replace the static coverage2D with the dynamically flaring one.
	// float macroVolume = dynamicCoverage * heightGradient; //[cite: 3]

	float macroVolume = coverage2D * heightGradient;
	return macroVolume;
}

// Warp cloud position away from the camera's view axis (capsule-based sliding warp)
// Returns the warped position and a fade factor for density
vec3 getWarpedCloudPos(vec3 p, out float fade) {
	fade = 1.0;
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


float sdExtrusion(vec3 p, float sdf2d, float h) {
    vec2 w = vec2(sdf2d, abs(p.y) - h);
    return min(max(w.x, w.y), 0.0) + length(max(w, 0.0));
}

float getCloudCoverageFromSDF(float sdf, float worldScale) {
	// SDF is negative inside the cloud, positive outside.
	// We want coverage to be 1.0 inside and 0.0 outside.
	// Use a smooth transition of 500m * worldScale.
	return 1.0 - smoothstep(-500.0 * worldScale, 500.0 * worldScale, sdf);
}

// Simple inline hash for cell-specific variation
float hash_sdf(vec3 p) {
	return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

// --- Configurable Analytical SDF Sphere Parameters ---
const int NUM_CL_SPHERES = 3;

const vec2 CL_SPHERE_CENTERS[3] = vec2[](
	vec2(0.25, 0.25),
	vec2(0.75, 0.35),
	vec2(0.5,  0.75)
);

const float CL_SPHERE_RADII[3] = float[](
	0.20,
	0.22,
	0.21
);

const float CL_RADIUS_VARIATION = 0.04;

float dot2(vec3 x) {
	return dot(x, x);
}

float sdSphere( vec3 p, float r )
{
  return length(p) - r;
}

float sdSolidAngle( vec3 p, vec2 c, float ra )
{
  // c is the sin/cos of the angle
  vec2 q = vec2( length(p.xz), p.y );
  float l = length(q) - ra;
  float m = length(q - c*clamp(dot(q,c),0.0,ra) );
  return max(l,m*sign(c.y*q.x-c.x*q.y));
}

float sdCutSphere( vec3 p, float r, float h )
{
  float w = sqrt(r*r-h*h);

  vec2 q = vec2( length(p.xz), p.y );
  float s = max( (h-r)*q.x*q.x+w*w*(h+r-2.0*q.y), h*q.x-w*q.y );
  return (s<0.0) ? length(q)-r :
         (q.x<w) ? h - q.y     :
                   length(q-vec2(w,h));
}

float sdRoundCone( vec3 p, vec3 a, vec3 b, float r1, float r2 )
{
  vec3  ba = b - a;
  float l2 = dot(ba,ba);
  float rr = r1 - r2;
  float a2 = l2 - rr*rr;
  float il2 = 1.0/l2;

  vec3 pa = p - a;
  float y = dot(pa,ba);
  float z = y - l2;
  float x2 = dot2( pa*l2 - ba*y );
  float y2 = y*y*l2;
  float z2 = z*z*l2;

  // single square root!
  float k = sign(rr)*rr*rr*x2;
  if( sign(z)*a2*z2>k ) return  sqrt(x2 + z2)        *il2 - r2;
  if( sign(y)*a2*y2<k ) return  sqrt(x2 + y2)        *il2 - r1;
                        return (sqrt(x2*a2*il2)+y*rr)*il2 - r1;
}

float sdVesicaSegment( in vec3 p, in vec3 a, in vec3 b, in float w )
{
    vec3  c = (a+b)*0.5;
    float l = length(b-a);
    vec3  v = (b-a)/l;
    float y = dot(p-c,v);
    vec2  q = vec2(length(p-c-y*v),abs(y));

    float r = 0.5*l;
    float d = 0.5*(r*r-w*w)/w;
    vec3  h = (r*q.x<d*(q.y-r)) ? vec3(0.0,r,0.0) : vec3(-d,0.0,d+w);

    return length(q-h.xy) - h.z;
}


// Elegant and precise analytical 3D sphere SDF

float evalSdf(
	vec3            p,
	float           time,
	CloudProperties props
) {
	if (time > 0.0) {
		p -= getCloudAdvectionOffset(0.0, time);
	}

	float tileScale = 1000.0 * worldScale;

	float y_min = cloudAltitude * worldScale;
	float y_max = (cloudAltitude + cloudThickness * 12.0) * worldScale;
	float H = y_max - y_min;

	vec2 p_xz = p.xz / tileScale;
	vec2 i_xz = floor(p_xz);

	float d = 1e10;

	// Determine how many tiles the 3D texture covers.
	// mapRange (1000) / tileScale (1000) = 1.0 tile period.
	float tilePeriod = 6.0;

	for (int dx = -1; dx <= 1; dx++) {
		for (int dz = -1; dz <= 1; dz++) {
			vec2 cell = i_xz + vec2(float(dx), float(dz));

			// 1. WRAP the cell coordinate to match the texture's repeat frequency
			// GLSL mod() safely handles negative coordinate wrapping
			vec2 wrapped_cell = mod(cell, tilePeriod);
			float rotHash = hash_sdf(vec3(wrapped_cell, 99.0));
			int rotSteps = int(rotHash * 4.0);

			for (int s = 0; s < NUM_CL_SPHERES; s++) {
				vec2 center_local = CL_SPHERE_CENTERS[s];
				float baseRadius = CL_SPHERE_RADII[s] * tileScale;

				vec2 c = center_local - vec2(0.5);
				if (rotSteps == 1)      c = vec2(-c.y,  c.x);
				else if (rotSteps == 2) c = vec2(-c.x, -c.y);
				else if (rotSteps == 3) c = vec2( c.y, -c.x);
				center_local = c + vec2(0.5);

				// 2. Hash using the WRAPPED cell so the layout repeats perfectly
				float h = hash_sdf(vec3(wrapped_cell, float(s)));

				float radius = baseRadius + (fract(h * 31.39) * 2.0 - 1.0) * CL_RADIUS_VARIATION * tileScale;
				float h_y = fract(h * 17.31);
				float Cy = (y_min + radius) + h_y * (H - 2.0 * radius);

				vec3 sphere_center;
				// 3. Place using the UNWRAPPED cell so the geometry exists at this world position
				sphere_center.xz = (cell + center_local) * tileScale;
				sphere_center.y = Cy;

				float dist;
				if (h < 0.6) {
					// Evaluate as Rounded, Capped Cone oriented to the direction of advection
					float angle = 3.14159265;
					vec3 advectDir = normalize(vec3(cos(angle), 0.0, sin(angle)));
					float halfLength = radius * 1.2;
					vec3 a = sphere_center - advectDir * halfLength;
					vec3 b = sphere_center + advectDir * halfLength;
					float r1 = radius * 1.1; // larger radius
					float r2 = radius * 0.5; // smaller radius (tapered)
					dist = sdRoundCone(p, a, b, r1, r2);
				} else if (h < 0.85) {
					// Evaluate as Sphere
					dist = length(p - sphere_center) - radius;
				} else {
					// Evaluate as Rounded Box (Anvil cloud)
					vec3 boxExtents = vec3(radius * 1.5, radius * 0.5, radius * 1.5);
					vec3 q = abs(p - sphere_center) - boxExtents;
					dist = length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - (radius * 0.2); // 0.2 is the corner rounding
				}

				d = smin(d, dist, 0.45 * tileScale);
			}
		}
	}
	float maxErosion = 500.0 * worldScale;
	float maxDilation = 500.0 * worldScale;
	float coverageOffset = mix(-maxErosion, maxDilation, props.coverage);
	return d - coverageOffset;
	// return d;
}

/**
 * Internal helper to calculate a "puffy" 3D SDF for a cloud point.
 */
float calculatePuffyCloudSDF(vec3 p, CloudWeather weather, CloudLayer layer, float worldScale) {
	// Representative "unit" for the height remapping
	float h_unit = 10000.0 * worldScale;

	// Normalize height relative to cloud center
	float cloudCenter = (layer.baseFloor + layer.thickness) * 0.5;
	float h = (p.y - cloudCenter) / h_unit;

	// Puff factor
	float R = 0.5;

	float x = weather.sdf / h_unit;
	float puffy_x = x + R;
	float d3d = h_unit * (length(vec2(max(0.0, puffy_x), h)) - R + min(0.0, puffy_x));


	return d3d;
}




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
	float h = getCloudRelativeHeight(p, weather, layer);

	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);
	vec3 advect = time * advectSpeed;
	vec3 p_advected = p + advect;

	float warpy = fastFbm3d(p_advected/5000.0);
	vec3 warpOffset = vec3(warpy) * 1500.0 * props.worldScale;
	// float baseSdf = calculatePuffyCloudSDF(p + warpOffset, weather, layer, props.worldScale);
	float baseSdf = 1.0;//calculateLoftedCloudSDF(p + warpOffset, weather, layer, props.worldScale);

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

CloudSpotDetails calculateCloudDensityExpV9(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	float            simplified
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

	float baseNoise = getCloud3DCoverage(p_advected, weather, layer, props.worldScale);

	// // weather.sdf now holds your 0.0 - 1.0 coverage probability from the bake shader
	// float coverage2D = weather.sdf;
	// float macroVolume = coverage2D * heightGradient;
	// float domeMask = smoothstep(h, h + 0.2, coverage2D);

	// macroVolume *= domeMask;
	// // float density = weather.density * smoothstep(0, weather.ecentricity, h) * (1.0 - smoothstep(weather.ecentricity, 1.0, h));

	// float baseNoise = 1.0;
	// // float baseSdf = getCloud3DCoverage(p_advected, weather, layer, props.worldScale);
	// baseNoise = macroVolume;

	// baseNoise *= heightGradient;
	// baseNoise = adjust(baseNoise, weather.density);
	// baseNoise = smoothstep(0.0, density, baseNoise);

	// bool isCore = baseNoise >= 0.50;

	// float erodeMask = 1.0-smoothstep(0.0, 0.90, baseNoise);
	float erodeMask = 1.0 - baseNoise;
	if (erodeMask > 0.0) {
		float largeScale = abs(fastFbm3d(p_advected/10000)) * erodeMask;
		baseNoise = adjust(baseNoise, largeScale);

		if (simplified < 1.0 && erodeMask > 0.0) {
			erodeMask = 1.0 - baseNoise;
			float coarseScale = abs(fastFbm3d(p_advected/5000.0)) * erodeMask;
			baseNoise = adjust(baseNoise, coarseScale);
		}

		if (simplified < .75 && erodeMask > 0.0) {
			erodeMask = 1.0 - baseNoise;
			float mediumScale = (1.0-fastRidge3d(p_advected/4000)) * erodeMask;
			baseNoise = adjust(baseNoise, mediumScale);
		}

		if (simplified < 0.50 && erodeMask > 0.0) {
			erodeMask = 1.0 - baseNoise;
			float fineScale = abs(fastFbm3d(p_advected / 3000.0)) * erodeMask;
			baseNoise = adjust(baseNoise, fineScale);
		}

		if (simplified < 0.25 && erodeMask > 0.0) {
			erodeMask = 1.0 - baseNoise;
			float detailScale = fastRidge3d(p / vec3(2000.0, 1000.0, 2000.0)) * erodeMask;
			baseNoise = adjust(baseNoise, detailScale);
		}
		baseNoise *= smoothstep(0.01, 0.32, baseNoise);
	}

	return CloudSpotDetails(
		clamp(baseNoise, 0.00, 1.0),
		vec3(1.0),
		advectSpeed
	);
}

#ifndef HELPERS_CLOUD_WEATHER_UTILS_GLSL
#define HELPERS_CLOUD_WEATHER_UTILS_GLSL

// Tile-aware 2D hash
vec2 hash2(vec2 p, vec2 period)
{
	if (period.x > 0.0) p = mod(p, period);
	p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
	return fract(sin(p) * 43758.5453123);
}

// Tile-aware hash returning a single float
float hash12(vec2 p, vec2 period) {
	if (period.x > 0.0) p = mod(p, period);
	vec3 p3 = fract(vec3(p.xyx) * .1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

// 2D Value Noise returning vec3(value, ddx, ddy)
vec3 valueNoiseGrad(vec2 p, vec2 period) {
	vec2 i = floor(p);
	vec2 f = fract(p);

	// Quintic Hermite interpolation for smooth second derivatives
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	vec2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);

	float a = hash12(i + vec2(0.0, 0.0), period);
	float b = hash12(i + vec2(1.0, 0.0), period);
	float c = hash12(i + vec2(0.0, 1.0), period);
	float d = hash12(i + vec2(1.0, 1.0), period);

	float k0 = a;
	float k1 = b - a;
	float k2 = c - a;
	float k3 = a - b - c + d;

	float val = k0 + k1 * u.x + k2 * u.y + k3 * u.x * u.y;

	// Compute the analytical gradient
	vec2 grad = du * vec2(k1 + k3 * u.y, k2 + k3 * u.x);

	return vec3(val, grad);
}

// FBM SDF generation
float calculateFbmSdf(vec2 p, float coverageThreshold, int octaves, vec2 period) {
	float val = 0.0;
	vec2 grad = vec2(0.0);

	float amp = 0.5;
	float freq = 1.0;

	for (int i = 0; i < octaves; i++) {
		vec3 n = valueNoiseGrad(p * freq, period * freq);

		val += amp * n.x;

		// Chain rule: scale the gradient by the amplitude and frequency
		grad += amp * freq * n.yz;

		amp *= 0.5;
		freq *= 2.0;

		// Optional: Domain rotation matrix here to break up axis alignment
	}

	// Divide the implicit surface by its gradient magnitude
	// Adding a tiny epsilon prevents division by zero at gradient singularities
	float distance = (val - coverageThreshold) / (length(grad) + 0.0001);

	return distance;
}

// Tile-aware 2D value noise for domain warping
vec2 warpNoise(vec2 p, vec2 period)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);

	vec2 a = hash2(i + vec2(0, 0), period);
	vec2 b = hash2(i + vec2(1, 0), period);
	vec2 c = hash2(i + vec2(0, 1), period);
	vec2 d = hash2(i + vec2(1, 1), period);

	// Remap to [-1, 1] range for displacement
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y) * 2.0 - 1.0;
}

struct VoronoiData {
	float dist;
	float dist_f1;
	float dist_f2;
	vec2 f1;
	vec2 f2;
};

// Exact Voronoi distance to cell edges
VoronoiData sdVoronoiEdge(vec2 p, vec2 period)
{
	vec2 n = floor(p);
	vec2 f = fract(p);

	vec2 mg, mr, mo;
	float md = 8.0;

	// Pass 1: Find closest point
	for (int j = -1; j <= 1; j++)
		for (int i = -1; i <= 1; i++)
		{
			vec2 g = vec2(float(i), float(j));
			vec2 o = hash2(n + g, period);
			// o = 0.5 + 0.5 * sin(iTime + 6.2831 * o);
			vec2 r = g + o - f;
			float d = dot(r, r);
			if (d < md) {
				md = d;
				mr = r;
				mg = g;
				mo = o;
			}
		}

	// Pass 2: Exact distance to the bisector plane
	md = 8.0;
	vec2 f2_g, f2_o;
	for (int j = -2; j <= 2; j++)
		for (int i = -2; i <= 2; i++)
		{
			vec2 g = mg + vec2(float(i), float(j));
			vec2 o = hash2(n + g, period);
			vec2 r = g + o - f;

			if (dot(mr - r, mr - r) > 0.00001) {
				float edgeDist = dot(0.5 * (mr + r), normalize(r - mr));
				if (edgeDist < md)
				{
					md = edgeDist;
					f2_g = g;
					f2_o = o;
				}
			}
		}

	VoronoiData res;

	res.f1 = n + mg + mo;
	res.f2 = n + f2_g + f2_o;

	res.dist = md;
	res.dist_f1 = distance(res.f1, f);
	res.dist_f2 = distance(res.f2, f);

	return res;
}

VoronoiData sdVoronoiFbm(vec2 p, float lacunarity, float iter, float coverage, vec2 period)
{
	vec2 warp = vec2(0.0);
	float amp = 0.5;
	vec2 wp = p;
	vec2 wPeriod = period;

	// Accumulate warp FBM
	for (float i = 0.0; i < iter; i++)
	{
		warp += amp * warpNoise(wp, wPeriod);
		wp *= lacunarity;
		wPeriod *= lacunarity;
		amp *= 0.5;
	}

	vec2 warpedP = p + warp * 0.5;
	VoronoiData res = sdVoronoiEdge(warpedP, period);

	res.dist = -(res.dist - (1.0 - coverage));
	return res;
}

float getWarpedVoronoiDist(vec2 p, float lacunarity, float iter, float coverage, vec2 period) {
	return sdVoronoiFbm(p, lacunarity, iter, coverage, period).dist;
}

VoronoiData sdVoronoiHybrid(vec2 p, float lacunarity, float iter, float coverage, vec2 period) {
	VoronoiData vd = sdVoronoiFbm(p, lacunarity, iter, coverage, period);

	vec2 eps = vec2(0.001, 0.0);

	float dx = getWarpedVoronoiDist(p + eps.xy, lacunarity, iter, coverage, period) -
			   getWarpedVoronoiDist(p - eps.xy, lacunarity, iter, coverage, period);

	float dy = getWarpedVoronoiDist(p + eps.yx, lacunarity, iter, coverage, period) -
			   getWarpedVoronoiDist(p - eps.yx, lacunarity, iter, coverage, period);

	vec2 grad = vec2(dx, dy) / (2.0 * eps.x);

	vd.dist = vd.dist / (length(grad) + 0.0001);

	return vd;
}

float smin_bake( float a, float b, float k )
{
    k *= 4.0;
    float h = max( k-abs(a-b), 0.0 )/k;
    return min(a,b) - h*h*k*(1.0/4.0);
}

float generateOrganicCellSDF(vec2 p, float cellSize, vec2 period, float coverage) {
	float lCover = 1.0-sqrt((coverage)/3.1415);
    vec2 p_grid = p / cellSize;
    vec2 id = floor(p_grid);

    float globalDist = 1e10;

    // --- Tuning Knobs ---
    float k = 0.4; // Blend strength (higher = more blobby/organic)

    // Parent circle constraints
    float parentBaseRadius = 0.40;
    float parentRadiusVar = 0.15;
    float parentScatter = 0.4;

    // Child circle constraints
    float childBaseRadius = 0.20;
    float childRadiusVar = 0.15;
    float childScatter = 0.30;

    // 3x3 Search to ensure seamless SDF across cell boundaries
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 neighborId = id + vec2(float(x), float(y));
            vec2 cellOrigin = neighborId;

            // --- Coverage Integration ---
            // 1. Hash this cell to determine its natural density (0.0 to 1.0)
            float cellDensity = hash12(neighborId + 99.9, period);

            // 2. Bias the natural density by the global coverage uniform
            // A coverage of 0.5 leaves the grid naturally patchy.
			float localCoverage = clamp(cellDensity + (coverage * 2.0 - 1.0), 0.0, 1.0);

            // 3. Skip heavy math if the cell is completely empty
            if (localCoverage <= 0.01) continue;

            // 4. Scale the shapes so clouds physically shrink before disappearing
            float radiusMod = localCoverage;

            // Generate Parent Circle
            vec2 parentHash = hash2(neighborId, period);
            vec2 parentCenter = cellOrigin + 0.5 + (parentHash - 0.5) * parentScatter;
            float parentRadius = (parentBaseRadius + hash12(neighborId + 13.37, period) * parentRadiusVar) * radiusMod;

            float cellDist = length(p_grid - parentCenter) - parentRadius;

            // Generate 4 Child Circles
            for (int cy = 0; cy <= 1; cy++) {
                for (int cx = 0; cx <= 1; cx++) {
                    vec2 childIndex = vec2(float(cx), float(cy));
                    vec2 childOrigin = cellOrigin + childIndex * 0.5;

                    vec2 childSeed = neighborId * 4.0 + childIndex;
                    vec2 childHash = hash2(childSeed, period);

                    vec2 childCenter = childOrigin + 0.25 + (childHash - 0.5) * childScatter;
                    float childRadius = (childBaseRadius + hash12(childSeed + 42.0, period) * childRadiusVar) * radiusMod;

                    float childSdf = length(p_grid - childCenter) - childRadius;

                    // Blend child into the parent structure
                    cellDist = smin_bake(cellDist, childSdf, k);
                }
            }

            // Blend this entire cell structure into the global field
            globalDist = smin_bake(globalDist, cellDist, k);
        }
    }

    // Convert back to world space scale
    return globalDist * cellSize;
}

float evaluateCloudShadowDensityAtWorldPos(vec2 worldXZ, float time) {
	if (!u_useCloudShadowMap) return 0.0;
	vec4 lightSpacePos = u_cloudShadowMatrix * vec4(worldXZ.x, 0.0, worldXZ.y, 1.0);
	vec2 shadowUV = lightSpacePos.xy * 0.5 + 0.5;
	// Sample bottom layer (layer 7) for total optical depth through clouds, scaled appropriately
	float totalDensity = textureLod(u_cloudShadowTexture, vec3(shadowUV, 7.0), 0.0).r * 0.001 * cloudShadowOpticalDepthMultiplier / max(0.001, worldScale);
	return totalDensity;
}


#endif // HELPERS_CLOUD_WEATHER_UTILS_GLSL
