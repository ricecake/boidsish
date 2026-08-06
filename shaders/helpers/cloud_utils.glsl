#include "lygia/generative/psrdnoise.glsl"
#include "lygia/space/uncenter.glsl"

layout(binding = [[CLOUD_SHADOW_MAP_BINDING]]) uniform sampler2D u_cloudShadowTexture;
uniform mat4 u_cloudShadowMatrix;
uniform bool u_useCloudShadowMap;

struct CloudProperties {
	float altitude;
	float thickness;
	float densityBase;
	float coverage;
	float worldScale;
};

struct CloudWeather {
	vec3 p;
	float sdf;        // Signed Distance Field (world space)
	float density;
	float heightMap;  // Altitude variety
	float thickness;  // Thickness variety
	float ecentricity;
	float curve;
	float centerDist;
};

struct CloudLayer {
	float baseFloor;
	float baseCeiling;
	float thickness;
};

float schlickGain(float x, float g) {
	g = clamp(g, 0.001, 0.999);
	float absDiff = abs(2.0 * x - 1.0);
	float denominator = g + absDiff * (1.0 - 2.0 * g);
	return 0.5 + ((x - 0.5) * (1.0 - g)) / denominator;
}
float schlickBias(float x, float g) {
	// Guard inputs to safe analytical ranges
	float xx = clamp(x, 0.0, 1.0);
	float gg = clamp(g, 1e-4, 1.0 - 1e-4);

	// Convert bias parameter to Schlick formulation factor
	// Schlick's fast alternative: f(x) = x / ((1/a - 2) * (1.0 - x) + 1.0)
	float k = (1.0 / gg) - 2.0;
	return xx / (k * (1.0 - xx) + 1.0);
}


float getCurvedAltitude(vec3 p) {
	// return p.y;
	float R_earth = 6360.0 * 1000.0 * worldScale;
	vec3 earthCenter = vec3(viewPos.x, -R_earth, viewPos.z);
	return length(p - earthCenter) - R_earth;
}

bool intersectSphereLocal(vec3 ro, vec3 rd, float radius, out float t0, out float t1) {
	float b = dot(ro, rd);
	float rLen = length(ro);
	float c = (rLen - radius) * (rLen + radius);
	float det = b * b - c;
	if (det < 0.0)
		return false;
	det = sqrt(det);
	t0 = -b - det;
	t1 = -b + det;
	return true;
}

bool intersectCloudShell(vec3 ro, vec3 rd, float worldScale, out float t_start, out float t_end) {
	float R_earth = 6360.0 * 1000.0 * worldScale;
	float R_floor = R_earth + (cloudAltitude - 500.0) * worldScale;
	float R_ceiling = R_earth + (cloudAltitude + 2.0 * cloudThickness + 500.0) * worldScale;

	vec3 earthCenter = vec3(viewPos.x, -R_earth, viewPos.z);
	vec3 relRo = ro - earthCenter;

	t_start = 1e10;
	t_end = -1e10;

	float t0, t1;
	if (intersectSphereLocal(relRo, rd, R_ceiling, t0, t1)) {
		t_start = max(0.0, t0);
		t_end = t1;

		if (intersectSphereLocal(relRo, rd, R_floor, t0, t1)) {
			if (t0 < 0.0) {
				t_start = max(t_start, t1);
			} else {
				t_end = min(t_end, t0);
			}
		}
		return t_start < t_end;
	}
	return false;
}

float getCloudRelativeHeight(vec3 p, CloudWeather weather, CloudLayer layer, out float localFloor, out float actualThickness) {
	float altitude = getCurvedAltitude(p);
	float altitudeShift = weather.heightMap * layer.thickness;
	float cellRange = 10000.0 * worldScale;
	// float thicknessTaper = clamp(1.0 - pow(weather.centerDist / (cellRange * 0.5), 2.0), 0.0, 1.0);
	// actualThickness = max(weather.thickness * layer.thickness * thicknessTaper, 10.0 * worldScale);
	actualThickness = max(weather.thickness * layer.thickness, 25.0 * worldScale);
	localFloor = layer.baseFloor + altitudeShift;
	return clamp((altitude - localFloor) / actualThickness, 0.0, 1.0);
}

float getCloudRelativeHeight(vec3 p, CloudWeather weather, CloudLayer layer) {
	float localFloor, actualThickness;
	return getCloudRelativeHeight(p, weather, layer, localFloor, actualThickness);
	// float altitude = getCurvedAltitude(p);
	// float altitudeShift = weather.heightMap * layer.thickness;
	// float actualThickness = weather.thickness * layer.thickness;
	// float localFloor = layer.baseFloor + altitudeShift;
	// return clamp((altitude - localFloor) / max(actualThickness, 0.001), 0.0, 1.0);
}

// float saturate(float value) {
// 	return clamp(value, 0.0, 1.0);
// }

vec4 hash41(float p) {
    vec4 p4 = fract(p * vec4(443.897, 441.423, .0973, .1099));
    p4 += dot(p4, p4.wzxy + 19.19);
    return fract((p4.xxyz + p4.yzzw) * p4.zywx);
}


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
	// imageStore(outWeatherMap, pixel, vec4(finalCoverage, distF1InMeters, cellID, density));

	CloudWeather weather;
	weather.p = p;

	float rawCoverage = 1.0-tex.r;
	// Apply props.coverage as an offset/threshold to the baked coverage map
	weather.sdf = clamp(clamp(1.0-tex.r, 0, 1) + (props.coverage * 2.0 - 1.0), 0.0, 1.0);
	weather.heightMap = tex.g;
	weather.thickness = tex.b;
	weather.density = tex.a * props.densityBase;
	if (props.coverage >= 1.0) {
		weather.sdf = 1.0;
		// weather.heightMap = 1.0;
		// weather.thickness = 1.0;
		weather.density = props.densityBase;
	}
	// float thickness = clamp(posNoise(p, mapRange, 5), 0.0, (1.0-cellData.f1_dist)*0.25*uCloudThickness/mapRange);

	weather.thickness = clamp(weather.thickness, 0, 10000/props.thickness * pow(weather.sdf, 0.75));
	// weather.thickness = schlickBias(weather.sdf, 0.25);
	// weather.density *= smoothstep(0.0, 0.95, weather.thickness);
	weather.density = mix(weather.density, weather.sdf * props.densityBase, 0.4);

	// weather.thickness = mix(weather.thickness, weather.density, 0.8);
	weather.heightMap = mix(weather.heightMap, 0.0, weather.thickness * 0.9);

	// weather.heightMap = clamp(tex.g, 0.01, 1.0);
	// weather.thickness = clamp(tex.b, 0.01, 1.0);
	// weather.density = clamp(tex.a, 0.01, 1.0);
	// weather.ecentricity = uncenter(psrdnoise(p, vec3(10.0)));
	// weather.curve = uncenter(psrdnoise(p/2.0, vec3(10.0)));
	// weather.centerDist = uncenter(psrdnoise(p/3.0, vec3(10.0)));

	return weather;
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props, float lod) {
	vec3 advect = getCloudWindOffset(time);
	vec3 p_advected = p + advect;

	// Use baked weather map. Sampling UV is worldXZ / range.
	// Range is 100,000 * worldScale as defined in the bake shader.
	vec2 uv = p_advected.xz / (100000.0 * props.worldScale);
	vec4 bakedWeather = textureLod(u_cloudWeatherTexture, uv, clamp(lod * 6.0, 0.0, 6.0));
	return loadCloudWeather(p, props, bakedWeather);
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props) {
	return computeCloudWeather(p, props, 0.0);
}

CloudLayer computeCloudLayer(CloudWeather weather, CloudProperties props) {
	CloudLayer layer;
	layer.baseFloor = props.altitude * props.worldScale;
	layer.thickness = max(props.thickness, 0.001) * props.worldScale;
	// The evaluation volume needs twice the thickness to account for a full height cloud at maximum altitude.
	layer.baseCeiling = layer.baseFloor + 2.0 * layer.thickness;
	return layer;
}

float getDensityHeightGradient(float h, float type) {
	// 1. Cumulonimbus (Type ~ 0.0): Massive storm chunks
	// Solid, dark, flat base with a dense core that tapers slightly near the anvil top.
	float cumulonimbus = smoothstep(0.0, 0.05, h) * (1.0 - smoothstep(0.7, 1.0, h));

	// 2. Cumulus (Type ~ 0.5): Puffy fair-weather clouds
	// Rounded bottoms and very rounded, billowy tops.
	float cumulus = smoothstep(0.0, 0.2, h) * (1.0 - smoothstep(0.6, 0.9, h));

	// 3. Stratus (Type ~ 1.0): Thin, patchy plates
	// Soft fade in, flat middle, soft fade out.
	float stratus = smoothstep(0.0, 0.3, h) * (1.0 - smoothstep(0.7, 1.0, h));

	float res = mix(cumulonimbus, cumulus, smoothstep(0.0, 0.5, type));
	res = mix(res, stratus, smoothstep(0.5, 1.0, type));

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



float getCloud3DSDF(vec3 p, CloudWeather weather, CloudLayer layer, float worldScale) {
	float localFloor, actualThickness;
	float h = getCloudRelativeHeight(p, weather, layer, localFloor, actualThickness);

	if (p.y < localFloor || p.y > (localFloor + actualThickness)) {
		return 0.0;
	}

	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);

	float coverage2D = weather.sdf; //

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

	// Beer's law approximation for the shadow density using 3D cloud coverage/density [0.0, 1.0].
	// Adjust contrast/sharpness of the cloud shadow edge using cloudShadowStepMultiplier,
	// and apply a slant factor for longer paths at oblique angles.
	float slant = 1.0 / max(0.01, L.y);
	float shadowDepth = (d3d / max(0.1, cloudShadowStepMultiplier)) * slant * 4.0;
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
	return d3d * 4.0;
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

	// Soft transition/penumbra for local occlusion based on 3D cloud coverage/density [0.0, 1.0].
	// Multiplied by 0.8 to preserve the 5x softer ratio compared to direct shadows (4.0 / 5.0 = 0.8).
	float occlusionDepth = (d3d / max(0.1, cloudShadowStepMultiplier)) * 0.8;

	// Dampen the ambient factor based on cloud shadow intensity
	float cloudAO = exp(-occlusionDepth * 2.0 * cloudShadowOpticalDepthMultiplier);

	return mix(1.0, cloudAO, cloudShadowIntensity);
}

