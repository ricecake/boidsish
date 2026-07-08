#version 460 core

out vec4 FragColor;
layout(location = 1) out vec4 Velocity;
layout(location = 2) out vec4 NormalOut;
layout(location = 3) out vec4 AlbedoOut;

in vec2 TexCoords;

#include "helpers/lighting.glsl"
#include "atmosphere/common.glsl"
#include "helpers/fast_noise.glsl"
#include "helpers/clouds.glsl"
#include "helpers/astral.glsl"

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


void main() {
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

	vec3 sunDir = vec3(0, 1, 0);
	vec3 sunColor = vec3(1);
	if (num_lights > 0) {
		sunDir = normalize(-lights[0].direction);
		sunColor = lights[0].color;
	}

	// 1. Atmospheric Scattering
	vec3 skyRadiance = sampleSkyView(world_ray);

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
	float starVisibility = smoothstep(0.5, 0.05, skyBrightness);
	vec3 stars = vec3(0);

	if (starVisibility > 0.0) {
		stars = computeStars(world_ray, time) * vec3(1.0, 0.9, 0.8);
	}

	// Attenuate stars by sky brightness — on Earth, stars are overwhelmed by
	// scattered sunlight during the day, not just absorbed
	// Note: Nebula is now part of skyRadiance (via SkyViewLUT)
	vec3 skyTransmittance = getTransmittance(r, world_ray.y);
	vec3 spaceBackground = stars * skyTransmittance * starVisibility;

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
	float phasedMask = moonMask * mix(0.08, 1.0, moonIllumination);

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
