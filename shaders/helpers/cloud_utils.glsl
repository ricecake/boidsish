struct CloudProperties {
	float altitude;
	float thickness;
	float densityBase;
	float coverage;
	float worldScale;
};

struct CloudWeather {
	float sdf;        // Signed Distance Field (world space)
	float heightMap;  // Altitude variety
	float thickness;  // Thickness variety
	float cellID;     // Per-cell variety
	vec3 p;
};

struct CloudLayer {
	float baseFloor;
	float baseCeiling;
	float thickness;
};

float getCurvedAltitude(vec3 p) {
	float R_earth = 6360.0 * 1000.0 * worldScale;
	vec3 earthCenter = vec3(viewPos.x, -R_earth, viewPos.z);
	return length(p - earthCenter) - R_earth;
}

// float saturate(float value) {
// 	return clamp(value, 0.0, 1.0);
// }

float remap(float value, float valueMin, float valueMax) {
	return (value - valueMin) / (valueMax - valueMin);
}

float remapClamp(float value, float inMin, float inMax, float outMin, float outMax) {
    float t = clamp((value - inMin) / (inMax - inMin), 0.0, 1.0);
    return mix(outMin, outMax, t);
}

float adjust(float value, float scaly) {
	float f = 1.0 - value;
	float h = 0.4; // adjustable filter

	float a = scaly * (1.0-h) + h;
	return clamp((remap(a, f, f + h)), 0.0, 1.0);
}

float cloudPhase(float cosTheta) {
	// Dual-lobe Henyey-Greenstein for forward and back scattering
	// Blended with a large isotropic component to ensure visibility at all angles
	float hg = mix(henyeyGreenstein(cloudPhaseG1, cosTheta), henyeyGreenstein(cloudPhaseG2, cosTheta), cloudPhaseAlpha);
	return mix(hg, (1.0 / (4.0 * PI)), cloudPhaseIsotropic);
}

float beerPowder(float d, float local_d) {
	// Approximation of multiple scattering (Beer-Powder law)
	// Ensuring sunny side isn't black when d is small
	return max(
		exp(-d),
		exp(-d * cloudPowderScale) * cloudPowderMultiplier * (1.0 - exp(-local_d * cloudPowderLocalScale))
	);
}

vec3 beerPowder(vec3 d, vec3 local_d) {
	// Approximation of multiple scattering (Beer-Powder law)
	// Ensuring sunny side isn't black when d is small
	return max(
		exp(-d),
		exp(-d * cloudPowderScale) * cloudPowderMultiplier * (vec3(1.0) - exp(-local_d * cloudPowderLocalScale))
	);
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

vec3 getCloudWindSpeed(float time) {
	float angle = cloudFlowDirection;
	vec2  flowDir = vec2(cos(angle), sin(angle));
	return vec3(flowDir.x, 0.0, flowDir.y) * cloudFlowSpeed * worldScale * 10.0;
}

vec3 getCloudAdvectionSpeed(float h, float time) {
	float angle = cloudFlowDirection;
	vec2  flowDir = vec2(cos(angle), sin(angle));

	// Dramatic non-linear shear profile
	float shear = h * h * cloudFlowHeightScale * 1.0;

	vec3 advect = getCloudWindSpeed(time);
	advect.xz += flowDir * shear * worldScale * 10.0;

	return advect;
}

vec3 getCloudWindOffset(float time) {
	return time * getCloudWindSpeed(time);
}

vec3 getCloudAdvectionOffset(float h, float time) {
	return time * getCloudAdvectionSpeed(h, time);
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

// https://iquilezles.org/articles/smin
float smin( float a, float b, float k )
{
	float h = max(k-abs(a-b),0.0);
	return min(a, b) - h*h*0.25/k;
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

CloudWeather loadCloudWeather(vec3 p, CloudProperties props, vec4 tex) {
	CloudWeather weather;
	weather.sdf = tex.r;
	weather.heightMap = tex.g;
	weather.cellID = tex.b;
	weather.thickness = tex.a;
	weather.p = p;

	return weather;
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props, float lod) {
	vec3 advect = getCloudWindOffset(time);
	vec3 p_advected = p + advect;

	// Use baked weather map. Sampling UV is worldXZ / range.
	// Range is 100,000 * worldScale as defined in the bake shader.
	vec2 uv = p_advected.xz / (100000.0 * props.worldScale);
	vec4 bakedWeather = textureLod(u_cloudWeatherTexture, uv, lod);
	return loadCloudWeather(p, props, bakedWeather);
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props) {
	return computeCloudWeather(p, props, 0.0);
}

CloudLayer computeCloudLayer(CloudWeather weather, CloudProperties props) {
	CloudLayer layer;
	layer.baseFloor = props.altitude * props.worldScale;
	// ceiling is altitude + thickness + thickness: a cloud at relative altitude 0 with thickness 1 should span the "base" layer.
	// A tall cloud should be able to be at high altitude, so need to support a real ceiling of 2*thickness
	layer.baseCeiling = (props.altitude + 10.0*props.thickness) * props.worldScale;
	layer.thickness = max(props.thickness, 0.001);
	return layer;
}

float getDensityHeightGradient(float h, float type) {
	// Typical height profiles for different cloud types
	// Stratus: low and thin
	float stratus = smoothstep(0.0, 0.1, h) * (1.0 - smoothstep(0.15, 0.3, h));
	// Cumulus: medium height, billowy
	float cumulus = smoothstep(0.0, 0.2, h) * (1.0 - smoothstep(0.7, 0.9, h));
	// Cumulonimbus: tall, reaching the top
	float cumulonimbus = smoothstep(0.0, 0.05, h) * (1.0 - smoothstep(0.85, 1.0, h));

	// Interpolate based on cloud type (weather.heightMap)
	float res = mix(stratus, cumulus, smoothstep(0.0, 0.5, type));
	res = mix(res, cumulonimbus, smoothstep(0.5, 1.0, type));
	return res;
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

float calculateLoftedCloudSDF(vec3 p, CloudWeather weather, CloudLayer layer, float worldScale) {
    // 1. The 2D footprint distance (positive outside, negative inside)
    float d_edge = weather.sdf;

    // 2. Calculate the depth inside the cloud boundary
    float depthInside = max(0.0, -d_edge);

    // 3. Map internal depth to vertical height along the +Y axis.
    // 'puffSlope' determines how steep the sides of the cloud are.
    // A slope of 1.0 represents a 45-degree rise from the edge.
    float puffSlope = 1.1;
    float domeHeight = depthInside * puffSlope;

    // Clamp the height to the atmospheric layer's defined maximum thickness
    domeHeight = min(domeHeight, layer.thickness);

    // 4. Define the vertical bounds for this specific XZ column
    // This creates a flat bottom at baseFloor and a domed top.
    float columnCenter = layer.baseFloor + (domeHeight * 0.5);
    float d_vertical = abs(getCurvedAltitude(p) - columnCenter) - (domeHeight * 0.5);

    // 5. Intersect the 2D boundary with the dynamic 1D vertical boundary
    // The exact distance to the boundary is the maximum of the two orthogonal distances.
    float d3d = max(d_edge, d_vertical);

    return d3d;
}

uniform sampler3D u_cloud3DTexture;

float getCloud3DSDF(vec3 p, CloudWeather weather, CloudLayer layer, float worldScale) {
	#ifdef CLOUD_BAKE_SHADER
	// Analytical evaluation during baking
	float coverage = getCloudCoverageFromSDF(weather.sdf, worldScale);
	float totalHeight = layer.baseCeiling - layer.baseFloor;

	float floorShift = mix(0.4, 0.0, smoothstep(0.15, 0.5, coverage));
	float thicknessFraction = mix(0.1, 1.0, smoothstep(0.15, 0.6, coverage));

	float heightVar = weather.heightMap * 0.2;
	floorShift = clamp(floorShift + heightVar, 0.0, 0.8);

	float localFloor = layer.baseFloor + floorShift * totalHeight;
	float localThickness = thicknessFraction * totalHeight * mix(0.8, 1.2, weather.thickness);
	float localCeiling = min(layer.baseCeiling, localFloor + localThickness);

	float altitude = getCurvedAltitude(p);
	float h = clamp((altitude - localFloor) / max(1.0, localCeiling - localFloor), 0.0, 1.0);

	float sdf2d = weather.sdf;
	if (coverage > 0.4) {
		float bottomShrink = 1.0 - smoothstep(0.0, 0.4, h);
		sdf2d += bottomShrink * 400.0 * worldScale * (coverage - 0.4);

		float topExpand = smoothstep(0.7, 1.0, h);
		sdf2d -= topExpand * 600.0 * worldScale * (coverage - 0.4);
	}

	float centerY = (localFloor + localCeiling) * 0.5;
	float halfHeight = (localCeiling - localFloor) * 0.5;
	float distY = abs(altitude - centerY) - halfHeight;

	vec2 w = vec2(sdf2d, distY);
	return min(max(w.x, w.y), 0.0) + length(max(w, 0.0));
	#else
	// Sampling from the precomputed B channel of the 3D volume texture
	float altitude = getCurvedAltitude(p);
	float h_volume = (altitude - layer.baseFloor) / (layer.baseCeiling - layer.baseFloor);

	if (h_volume < -0.1 || h_volume > 1.1) {
		// Analytical fallback when far outside the volume
		float coverage = getCloudCoverageFromSDF(weather.sdf, worldScale);
		float totalHeight = layer.baseCeiling - layer.baseFloor;

		float floorShift = mix(0.4, 0.0, smoothstep(0.15, 0.5, coverage));
		float thicknessFraction = mix(0.1, 1.0, smoothstep(0.15, 0.6, coverage));

		float heightVar = weather.heightMap * 0.2;
		floorShift = clamp(floorShift + heightVar, 0.0, 0.8);

		float localFloor = layer.baseFloor + floorShift * totalHeight;
		float localThickness = thicknessFraction * totalHeight * mix(0.8, 1.2, weather.thickness);
		float localCeiling = min(layer.baseCeiling, localFloor + localThickness);

		float h = clamp((altitude - localFloor) / max(1.0, localCeiling - localFloor), 0.0, 1.0);

		float sdf2d = weather.sdf;
		if (coverage > 0.4) {
			float bottomShrink = 1.0 - smoothstep(0.0, 0.4, h);
			sdf2d += bottomShrink * 400.0 * worldScale * (coverage - 0.4);

			float topExpand = smoothstep(0.7, 1.0, h);
			sdf2d -= topExpand * 600.0 * worldScale * (coverage - 0.4);
		}

		float centerY = (localFloor + localCeiling) * 0.5;
		float halfHeight = (localCeiling - localFloor) * 0.5;
		float distY = abs(altitude - centerY) - halfHeight;

		vec2 w = vec2(sdf2d, distY);
		return min(max(w.x, w.y), 0.0) + length(max(w, 0.0));
	}

	vec3 advectSpeed = getCloudAdvectionSpeed(clamp(h_volume, 0.0, 1.0), time);
	vec3 advect_3d = time * advectSpeed * 0.75;
	vec3 p_advected_3d = p + advect_3d;
	vec3 uvw = vec3(
		p_advected_3d.x / (10000.0 * worldScale),
		clamp(h_volume, 0.0, 1.0),
		p_advected_3d.z / (50000.0 * worldScale)
	);
	vec4 volSample = textureLod(u_cloud3DTexture, uvw, 0.0);
	return volSample.b;
	#endif
}


/**
 * Calculate cloud shadow factor for a fragment position.
 * Projects the fragment to the cloud layer and samples the weather map SDF directly.
 */
float calculateCloudShadowFactor(vec3 frag_pos, vec3 L, float intensity) {
	if (intensity <= 0.0) return 1.0;
	if (L.y <= 0.001) return 1.0;

	// Project to the cloud center height to find the casting XZ position
	float centerHeight = (cloudAltitude + cloudThickness * 0.5) * worldScale;
	if (frag_pos.y > centerHeight)
		return 1.0;

	// Project to the cloud center height along the light ray
	float t = (centerHeight - frag_pos.y) / L.y;
	vec3 cloudPos = frag_pos + L * t;

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	CloudWeather weather = computeCloudWeather(cloudPos, props);
	CloudLayer layer = computeCloudLayer(weather, props);

	float d3d = getCloud3DSDF(cloudPos, weather, layer, props.worldScale);

	// Sharpness of the cloud edge in meters, scaled by cloudShadowStepMultiplier
	float penumbra = 100.0 * worldScale * max(0.1, cloudShadowStepMultiplier);

	// Beer's law approximation for the shadow density
	// We multiply by a factor to make the shadow more prominent
	// And apply a slant factor for longer paths at oblique angles
	float slant = 1.0 / max(0.01, L.y);
	float shadowDepth = max(0.0, -d3d / penumbra) * slant;
	float shadowTerm = exp(-shadowDepth * 8.0 * cloudShadowOpticalDepthMultiplier);

	return mix(1.0, shadowTerm, intensity);
}

float evaluateCloudShadowDensityAtWorldPos(vec2 worldXZ, float time) {
	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	vec3  basePos = vec3(worldXZ.x, (props.altitude + props.thickness * 0.5) * props.worldScale, worldXZ.y);
	CloudWeather weather = computeCloudWeather(basePos, props);
	CloudLayer layer = computeCloudLayer(weather, props);

	float d3d = getCloud3DSDF(basePos, weather, layer, props.worldScale);
	float penumbra = 100.0 * worldScale;
	return max(0.0, -d3d / penumbra) * 4.0;
	// return calculateCloudDensityExpV8(basePos, weather, layer, props, time, true).x;
}

/**
 * Calculate local ambient occlusion from clouds at a fragment position.
 * Smoothly dampens the sky/ambient factor where clouds are directly above.
 */
float calculateCloudAmbientOcclusion(vec3 frag_pos) {
	float centerHeight = (cloudAltitude + cloudThickness * 0.5) * worldScale;
	if (frag_pos.y > centerHeight) {
		return 1.0;
	}

	vec3 cloudPos = vec3(frag_pos.x, centerHeight, frag_pos.z);

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	// Use a high LOD (6.0) to get a low-resolution, smoothed/averaged representation of the cloud coverage above.
	CloudWeather weather = computeCloudWeather(cloudPos, props, 6.0);
	CloudLayer layer = computeCloudLayer(weather, props);

	float d3d = getCloud3DSDF(cloudPos, weather, layer, props.worldScale);

	// Soft penumbra/transition for local occlusion
	float penumbra = 500.0 * worldScale * max(0.1, cloudShadowStepMultiplier);
	float occlusionDepth = max(0.0, -d3d / penumbra);

	// Dampen the ambient factor based on cloud shadow intensity
	float cloudAO = exp(-occlusionDepth * 2.0 * cloudShadowOpticalDepthMultiplier);

	return mix(1.0, cloudAO, cloudShadowIntensity);
}

