#version 460 core

out vec4 FragColor;
layout(location = 1) out vec4 Velocity;
layout(location = 2) out vec4 NormalOut;
layout(location = 3) out vec4 AlbedoOut;

in vec2 TexCoords;

#include "helpers/terrain_common.glsl"
#include "helpers/lighting.glsl"
#include "atmosphere/common.glsl"
#include "helpers/fast_noise.glsl"
#include "helpers/clouds.glsl"
#include "helpers/astral.glsl"
#include "visual_effects.glsl"

layout(binding = [[TERRAIN_COLOR_BLEND_BINDING]]) uniform sampler3D u_terrainColorBlend;

float evalNoiseHeightAndGradient(vec2 p, out vec2 grad) {
	vec2 g1, g2;
	vec2 scale1 = vec2(0.005 / worldScale);
	vec2 scale2 = vec2(0.015 / worldScale);

	float n1 = psrdnoise(p * scale1, vec2(0.0), 0.0, g1);
	float n2 = psrdnoise(p * scale2, vec2(0.0), 0.0, g2);

	float h1 = n1 * 25.0 * worldScale;
	float h2 = n2 * 10.0 * worldScale;

	grad = g1 * scale1 * 25.0 * worldScale + g2 * scale2 * 10.0 * worldScale;
	return h1 + h2;
}

uniform mat4 invProjection;
uniform mat4 invView;

// u_transmittanceLUT is declared in helpers/lighting.glsl
uniform sampler2D u_skyViewLUT;

uniform vec3 u_sunRadiance; // Added for consistency with scattering
uniform vec3 u_moonRadiance;
uniform vec3 u_moonFullRadiance;
uniform vec3 u_moonDir;

vec3 getTransmittance(float r, float mu) {
	vec2 uv = transmittanceToUV(r, mu);
	return texture(u_transmittanceLUT, uv).rgb;
}

vec3 sampleSkyView(vec3 rd) {
	float elevation = asin(clamp(rd.y, -1.0, 1.0));
	float azimuth = atan(rd.x, -rd.z);
	if (azimuth < 0.0)
		azimuth += 2.0 * PI;

	// Non-linear mapping for better horizon detail
	float v = (elevation < 0.0) ? (0.5 - 0.5 * sqrt(-elevation / (PI * 0.5)))
								: (0.5 + 0.5 * sqrt(elevation / (PI * 0.5)));
	vec2  uv = vec2(azimuth / (2.0 * PI), v);
	return texture(u_skyViewLUT, uv).rgb;
}

// Simple hash function for Voronoi in the sky shader
vec2 sky_hash22(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453123);
}

// Distance-to-edge Voronoi for sharp solar flare loop/filament boundaries
float voronoiDistanceToEdgeSky(vec2 x) {
    vec2 n = floor(x);
    vec2 f = fract(x);

    vec2 mg, mr;
    float md = 8.0;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = sky_hash22(n + g);
            o = 0.5 + 0.5 * sin(time * 0.3 * solar_flare_speed + 6.2831 * o);
            vec2 r = g + o - f;
            float d = dot(r, r);

            if (d < md) {
                md = d;
                mr = r;
                mg = g;
            }
        }
    }

    md = 8.0;
    for (int j = -2; j <= 2; ++j) {
        for (int i = -2; i <= 2; ++i) {
            vec2 g = mg + vec2(float(i), float(j));
            vec2 o = sky_hash22(n + g);
            o = 0.5 + 0.5 * sin(time * 0.3 * solar_flare_speed + 6.2831 * o);
            vec2 r = g + o - f;

            if (dot(mr - r, mr - r) > 0.00001) {
                md = min(md, dot(0.5 * (mr + r), normalize(r - mr)));
            }
        }
    }
    return md;
}


void main() {
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

	// 1. Atmospheric Scattering
	vec3 skyRadiance = sampleSkyView(world_ray);

	if (world_ray.y < 0.0) {

		// Raymarch heightfield along world_ray
		float hMax = 35.0 * worldScale;
		float hMin = -35.0 * worldScale;
		float tStart = (viewPos.y > hMax) ? (hMax - viewPos.y) / world_ray.y : 0.0;
		float tEnd = (hMin - viewPos.y) / world_ray.y;
		tStart = max(0.0, tStart);
		tEnd = max(tStart + 0.1, tEnd);

		float t = tStart;
		float dt = (tEnd - tStart) / 10.0;
		float prevT = tStart;
		float prevHDiff = 0.0;
		bool hit = false;

		vec3 p = viewPos + t * world_ray;

		for (int i = 0; i <= 10; ++i) {
			p = viewPos + t * world_ray;
			float distToCam = length(p.xz - viewPos.xz);

			vec2 dummyGrad;
			float noiseH = evalNoiseHeightAndGradient(p.xz, dummyGrad);

			TerrainSurface terrainSurf = getTerrainSurface(p.xz);
			float terrainBlend = (terrainSurf.height > -9000.0)
				? (1.0 - smoothstep(100.0 * worldScale, 500.0 * worldScale, distToCam))
				: 0.0;

			float surfaceH = mix(noiseH, terrainSurf.height, terrainBlend);
			float hDiff = p.y - surfaceH;

			if (i > 0 && hDiff < 0.0) {
				float frac = clamp(prevHDiff / (prevHDiff - hDiff + 1e-5), 0.0, 1.0);
				t = mix(prevT, t, frac);
				p = viewPos + t * world_ray;
				hit = true;
				break;
			}

			prevT = t;
			prevHDiff = hDiff;
			t += dt;
		}

		if (!hit) {
			t = -max(0.001 * worldScale, viewPos.y) / world_ray.y;
			p = viewPos + t * world_ray;
		}

		float finalDist = length(p.xz - viewPos.xz);

		vec2 noiseGrad;
		float noiseH = evalNoiseHeightAndGradient(p.xz, noiseGrad);

		TerrainSurface terrainSurf = getTerrainSurface(p.xz);
		float terrainBlend = (terrainSurf.height > -9000.0)
			? (1.0 - smoothstep(100.0 * worldScale, 500.0 * worldScale, finalDist))
			: 0.0;

		float finalHeight = mix(noiseH, terrainSurf.height, terrainBlend);
		p.y = finalHeight;

		// Analytical noise normal from exact gradients
		vec3 noiseNorm = normalize(vec3(-noiseGrad.x, 1.0, -noiseGrad.y));
		vec3 norm = (terrainBlend > 0.0 && terrainSurf.height > -9000.0)
			? normalize(mix(noiseNorm, terrainSurf.normal, terrainBlend))
			: noiseNorm;

		// Sample 3D terrain color palette
		float heightNormalized = clamp(finalHeight / (100.0 * worldScale), 0.0, 1.0);
		float moisture = 0.5;
		float roughness = 0.8;
		vec3 surfaceColor = texture(u_terrainColorBlend, vec3(heightNormalized, moisture, roughness)).rgb;

		// Apply PBR lighting
		float primaryShadow;
		vec3 lighting = apply_lighting_pbr(p, norm, surfaceColor, roughness, 0.0, 1.0, primaryShadow).rgb;

		// Distance Fog / Horizon Sky blending
		float fogFactor = clamp(exp(-finalDist / (3000.0 * worldScale)), 0.0, 1.0);

		vec3 horizonRay = normalize(vec3(world_ray.x, 0.0, world_ray.z));
		vec3 horizonSkyRadiance = sampleSkyView(horizonRay);
		vec3 finalColor = mix(horizonSkyRadiance, lighting, fogFactor);

		// Add lightning background pulse
		vec3 localLightningEffect = lightningColor * lightningPulse * 0.35;
		finalColor += localLightningEffect;

		FragColor = vec4(finalColor, 1.0);
		Velocity = vec4(0.0, 0.0, roughness, 0.0);
		NormalOut = vec4(normalize(mat3(view) * norm), primaryShadow);
		AlbedoOut = vec4(surfaceColor, 1.0);
		return;
	}

	vec3 sunDir = vec3(0, 1, 0);
	vec3 sunColor = vec3(1);
	if (num_lights > 0) {
		sunDir = normalize(-lights[0].direction);
		sunColor = lights[0].color;
	}

	// Fetch weather scalars for humidity-driven effects
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2 weatherUV = (viewPos.xz / scaledChunkSize - vec2(u_originSize.xy)) / 128.0;
	vec4 weatherScalars = texture(u_weatherScalars, weatherUV);
	float localHumidity = weatherScalars.y;

	// 2. Sun Disc with Realistic Distortion (flattening near horizon)
	float cosTheta = dot(world_ray, sunDir);
	float sunAngularRadius = 0.02; // approx 1.0 degrees

	// Construct local sun frame
	vec3 sunUp = vec3(0, 1, 0);
	vec3 sunRight = normalize(cross(sunUp, sunDir));
	sunUp = cross(sunDir, sunRight);

	// Project ray into sun frame
	float rayLocalX = dot(world_ray, sunRight);
	float rayLocalY = dot(world_ray, sunUp);
	float rayLocalZ = dot(world_ray, sunDir);

	// Distort vertical axis near horizon
	float atmosphericRefraction = 1.0 + 0.6 * exp(-max(0.0, sunDir.y * 10.0));
	float flattenedY = rayLocalY * atmosphericRefraction;

	// Effective angle to sun center
	float distSq = rayLocalX * rayLocalX + flattenedY * flattenedY;
	float distToSun = sqrt(max(0.0, distSq));

	// Humidity-driven sun aureole (Mie scattering approximation)
	float aureoleNoise = pow((fbm_astral(vec3(rayLocalX*3, rayLocalY*3, time*0.01)) * 0.5) + 1.5, 3)/(1+distSq);
	localHumidity *= aureoleNoise;
	float aureoleScale = 1.1 + smoothstep(0.0, 1.00, localHumidity * sunAureoleStrength);
	float aureole = exp(-distToSun * (45.0 / (aureoleScale))) * sunAureoleStrength * 3.5 * (1.3 + 1.750 * localHumidity);

	float sunMask = 1.0 - smoothstep(
		(sunAngularRadius - 0.001) * (sunAngularRadius - 0.001),
		sunAngularRadius * sunAngularRadius,
		distSq
	);
	// Add aureole to the mask with soft-clamping to avoid hard cut-offs at high brightness
	// Using a more gradual quadratic-rational soft-clamp for a natural look
	sunMask += aureole;

	// Add dramatically oversized solar flares/prominences flowing radially outwards
	float solarFlares = 0.0;
	if (solar_flares_enabled == 1) {
		float theta = atan(rayLocalY, rayLocalX);
		float r_sun = length(vec2(rayLocalX, rayLocalY));

		// Warp polar components with Simplex noise for turbulent plasma motion
		vec3 warpPos = vec3(rayLocalX * 25.0 * solar_flare_scale, rayLocalY * 25.0 * solar_flare_scale, time * 0.1 * solar_flare_speed);
		float angleWarp = snoise3d(warpPos) * 0.5;
		float distWarp = snoise3d(warpPos + vec3(19.0, 29.0, 37.0)) * 0.06;

		float warpedTheta = theta + angleWarp;
		float warpedR = r_sun + distWarp;

		// Map to a radial Voronoi cell space, moving outward with time
		vec2 cellCoords = vec2(warpedTheta * 6.5, (warpedR - time * 0.06 * solar_flare_speed) * 18.0 * solar_flare_scale);
		float voronoiDist = voronoiDistanceToEdgeSky(cellCoords);

		// Create sharp filaments and thick prominence loops
		float filament = 1.0 - smoothstep(0.0, 0.09, voronoiDist);
		float loopArc = smoothstep(0.04, 0.45, voronoiDist);
		float prominence = max(filament * 0.95, loopArc * 0.2);

		// Radial decay starting from the sun surface (sunAngularRadius)
		// Oversized flares: extend decay range
		float flareDecay = exp(-max(0.0, r_sun - sunAngularRadius) * (14.0 / solar_flare_scale));

		solarFlares = prominence * flareDecay * solar_flare_strength * 2.0;
	}
	sunMask += solarFlares;

	sunMask = (sunMask * (1.0 + sunMask * 0.05)) / (1.0 + sunMask * 0.06);

	// Ensure we are in front of the sun
	sunMask *= step(0.0, rayLocalZ);

	float r = kEarthRadius + max(0.0, viewPos.y / (1000.0 * worldScale));
	vec3  sunTransmittance = max(getTransmittance(r, sunDir.y), vec3(0.001));
	// Use u_sunRadiance if available (via AtmosphereManager) or fallback to simple sunColor
	// We divide by PI for physical consistency if it's treated as irradiance
	vec3 radiance = length(u_sunRadiance) > 0.0 ? u_sunRadiance : sunColor;
	vec3 sunDisc = radiance * sunMask * sunTransmittance * smoothstep(-0.01, 0.01, sunDir.y);

	// 3. Stars and Nebula
	float skyBrightness = max(max(skyRadiance.r, skyRadiance.g), skyRadiance.b);
	float starVisibility = 1.0 - smoothstep(0.05, 0.5, skyBrightness);
	vec3 stars = vec3(0);
	vec3 nebula = vec3(0);
	vec3 skyTransmittance = vec3(0.0);

	if (starVisibility > 0.0) {
		skyTransmittance = getTransmittance(r, world_ray.y);
		if (any(greaterThan(skyTransmittance, vec3(0.0)))) {
			stars = computeStars(world_ray, time);
			nebula = computeNebula(world_ray, time);
		}
	}

	// Attenuate stars by sky brightness — on Earth, stars are overwhelmed by
	// scattered sunlight during the day, not just absorbed
	// Note: Nebula is now part of skyRadiance (via SkyViewLUT)
	vec3 spaceBackground = (stars + nebula) * skyTransmittance * starVisibility;

	// 4. Moon Disc with Atmospheric Refraction
	vec3  moonDir = normalize(u_moonDir);
	float cosMoon = dot(world_ray, moonDir);
	float moonAngularRadius = 0.018; // Slightly smaller than sun

	// Moon frame
	vec3 moonUp = vec3(0, 1, 0);
	vec3 moonRight = normalize(cross(moonUp, moonDir));
	moonUp = cross(moonDir, moonRight);

	float moonLocalX = dot(world_ray, moonRight);
	float moonLocalY = dot(world_ray, moonUp);
	float moonLocalZ = dot(world_ray, moonDir);

	// Moon refraction flattening
	float moonAtmRefraction = 1.0 + 0.6 * exp(-max(0.0, moonDir.y * 10.0));
	float moonFlattenedY = moonLocalY * moonAtmRefraction;

	float moonDistSq = moonLocalX * moonLocalX + moonFlattenedY * moonFlattenedY;
	float moonMask = smoothstep(
		moonAngularRadius * moonAngularRadius,
		(moonAngularRadius - 0.001) * (moonAngularRadius - 0.001),
		moonDistSq
	);

	moonMask *= step(0.99, moonLocalZ);

	vec3 moonTransmittance = max(getTransmittance(r, moonDir.y), vec3(0.001));

	// Realistic Spherical Moon Terminator
	// We treat the moon disc as the projection of a 3D sphere.
	// Normalized coordinates on the moon disc:
	float moonDiscRadius = moonAngularRadius;
	vec2  moonDiscCoord = vec2(moonLocalX, moonFlattenedY) / moonDiscRadius;
	float r2 = dot(moonDiscCoord, moonDiscCoord);

	// Calculate surface normal on the sphere at this point (facing observer)
	float z = sqrt(max(0.0, 1.0 - r2));
	vec3  moonSurfaceNormal = normalize(moonDiscCoord.x * moonRight + moonDiscCoord.y * moonUp + z * moonDir);

	// Lit side of the moon faces the sun.
	float moonIllumination = max(0.0, dot(moonSurfaceNormal, sunDir)) * 5.8;

	// Earthshine: dark side gets a faint glow (~8%)
	float phasedMask = moonMask * mix(0.01, 1.0, moonIllumination);

	vec3 moonDisc = u_moonFullRadiance * phasedMask * moonTransmittance * smoothstep(-0.01, 0.01, moonDir.y);

	// 5. Cirrus Cloud Layer
	vec3 cirrusColor = vec3(0.0);
	if (cirrusOpacity > 0.01) {
		float cirrusAlt = 10.0; // 10 km altitude
		float camAltKM = viewPos.y / (1000.0 * worldScale);
		float h_cirrus = cirrusAlt - camAltKM;

		if (world_ray.y > 0.0) {
			float t_cirrus = h_cirrus / max(world_ray.y, 0.001);
			vec3  p_cirrus = viewPos + world_ray * (t_cirrus * 1000.0 * worldScale);

			// Advect cirrus using global flow
			vec3 advect = getCloudWindOffset(time * 0.5); // Cirrus moves slower relative to world
			vec2 uv_cirrus = (p_cirrus.xz + advect.xz) * (0.00005 / worldScale);

			float n = (fbm_astral(vec3(uv_cirrus * 2.0, time * 0.01)) + 1.0) * 0.5;
			float n2 = (fbm_astral(vec3(uv_cirrus * 5.0, time * 0.02 + 10.0)) + 1.0) * 0.5;
			float noise = smoothstep(0.3, 0.8, n * n2);

			vec3  T_cirrus = texture(u_transmittanceLUT, transmittanceToUV(kEarthRadius + cirrusAlt, sunDir.y)).rgb;
			float cirrusPhase = mix(0.2, 1.0, pow(max(0.0, dot(world_ray, sunDir)), 3.0));

			// High-altitude cirrus receives strong scattered sky light even when noise is low
			vec3 cirrusLighting = (T_cirrus * sunColor * cirrusPhase * 5.0) + (skyRadiance * 0.5);
			cirrusColor = cirrusLighting * noise * cirrusOpacity * 15.0;

			// Fade cirrus near horizon to avoid tiling artifacts
			cirrusColor *= smoothstep(0.0, 0.15, world_ray.y);
		}
	}

	// Lightning background pulse
	vec3 lightningEffect = lightningColor * lightningPulse * 0.35;

	vec3 finalColor = skyRadiance + sunDisc + moonDisc + cirrusColor + spaceBackground + lightningEffect;

	FragColor = vec4(finalColor, 1.0);
	Velocity = vec4(0, 0, 1.0, 0.0); // Roughness 1.0 (sky is not reflective), Metallic 0.0
	NormalOut = vec4(0, 0, 0, 1.0);
	AlbedoOut = vec4(finalColor, 1.0);
}
