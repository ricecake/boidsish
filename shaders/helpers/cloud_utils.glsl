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
};

struct CloudLayer {
	float baseFloor;
	float baseCeiling;
	float thickness;
};

vec3 cloudPhase(float cosTheta) {
	// Dual-lobe Henyey-Greenstein for forward and back scattering
	// Blended with a large isotropic component to ensure visibility at all angles
	vec3 hg = mix(henyeyGreenstein(cloudPhaseG1, cosTheta), henyeyGreenstein(cloudPhaseG2, cosTheta), cloudPhaseAlpha);
	return mix(hg, vec3(1.0 / (4.0 * PI)), cloudPhaseIsotropic);
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

float evalSdf(vec3            p, float           time, CloudProperties props) {
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
				if (h < 100.5) {
					// Evaluate as Sphere
					dist = length(p - sphere_center) - radius;
					// dist = sdRoundCone(sphere_center, vec3(0.25, sphere_center.y, 0.25), vec3(0.5, sphere_center.y, 0.5), 1.25 * radius, 0.75 * radius);
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

const float cloudFlow = 3.14;
const float clFlowSpeed = 5.0;
vec3 getCloudWindSpeed(float time) {
	return vec3(0);
}

vec3 getCloudAdvectionSpeed(float h, float time) {
	return vec3(0);
}

vec3 getCloudWindOffset(float time) {
	return vec3(0);
}

vec3 getCloudAdvectionOffset(float h, float time) {
	return vec3(0);
}

CloudWeather loadCloudWeather(vec4 tex) {
	CloudWeather weather;
	weather.sdf = tex.r;
	weather.heightMap = tex.g;
	weather.cellID = tex.b;
	weather.thickness = tex.a;

	return weather;
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props) {
	CloudWeather weather;
	weather.sdf = evalSdf(p, time, props);
	weather.heightMap = 0.5;
	weather.cellID = 0.5;
	weather.thickness = 0.5;
	return weather;
}

CloudLayer computeCloudLayer(CloudWeather weather, CloudProperties props) {
	CloudLayer layer;
	layer.baseFloor = props.altitude * props.worldScale;
	layer.baseCeiling = layer.baseFloor + (props.thickness * 12.0) * props.worldScale;
	layer.thickness = max(layer.baseCeiling - layer.baseFloor, 0.001);
	return layer;
}

float remapClamp(float value, float inMin, float inMax, float outMin, float outMax) {
	float t = clamp((value - inMin) / (inMax - inMin), 0.0, 1.0);
	return mix(outMin, outMax, t);
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

vec3 getWarpedCloudPos(vec3 p, out float fade) {
	fade = 1.0;
	return p;
}

float getCloudCoverageFromSDF(float sdf, float worldScale) {
	return 1.0 - smoothstep(-500.0 * worldScale, 500.0 * worldScale, sdf);
}

/**
 * Calculate cloud shadow factor for a fragment position.
 * Projects the fragment to the cloud layer and samples the weather map SDF directly.
 */
vec3 calculateCloudShadowFactor(vec3 frag_pos, vec3 L, float intensity) {
	if (intensity <= 0.0) return vec3(1.0);
	if (L.y <= 0.001) return vec3(1.0);

	// Skip if fragment is above the cloud layer
	float y_min = cloudAltitude * worldScale;
	float y_max = (cloudAltitude + cloudThickness * 12.0) * worldScale;
	if (frag_pos.y > y_max)
		return vec3(1.0);

	// Project to the cloud center to find the casting XZ position
	float y_center = (y_min + y_max) * 0.5;
	float t = (y_center - frag_pos.y) / L.y;

	vec3 cloudPos = frag_pos + L * t;

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	float d3d = evalSdf(cloudPos, time, props);

	// Sharpness of the cloud edge in meters
	float penumbra = 100.0 * worldScale;

	// Beer's law approximation for the shadow density
	// We multiply by a factor to make the shadow more prominent
	// And apply a slant factor for longer paths at oblique angles
	float slant = 1.0 / max(0.01, L.y);
	float shadowDepth = max(0.0, -d3d / penumbra) * slant;
	vec3 rgbExtinction = cloudExtinctionColor * cloudExtinction;
	vec3 stepOD = shadowDepth * (rgbExtinction * 800.0);
	vec3 shadowTerm = exp(-stepOD);

	return mix(vec3(1.0), shadowTerm, intensity);
}

float evaluateCloudShadowDensityAtWorldPos(vec2 worldXZ, float time) {
	float y_min = cloudAltitude * worldScale;
	float y_max = (cloudAltitude + cloudThickness * 12.0) * worldScale;
	float y_center = (y_min + y_max) * 0.5;
	vec3 basePos = vec3(worldXZ.x, y_center, worldXZ.y);

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	float d3d = evalSdf(basePos, time, props);
	float penumbra = 100.0 * worldScale;
	return max(0.0, -d3d / penumbra) * 4.0;
}
